#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <drm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct testapp_ctx {
	int fd;
	drmModeRes *res;
	drmModePlaneRes *plane_res;
};

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s -a | -b\n"
		"  -a  list DRM resources (connector/crtc/encoder/plane)\n"
		"  -b  perform a simple legacy modeset on the first connected output\n",
		prog);
}

static int open_drm(void)
{
	const char *candidates[] = {
		"/dev/dri/card0",
		"/dev/dri/card1",
	};

	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
		int fd = open(candidates[i], O_RDWR | O_CLOEXEC);
		if (fd >= 0)
			return fd;
	}

	return -1;
}

static int open_virtio_gpu(void)
{
	const char *candidates[] = {
		"/dev/dri/card0",
		"/dev/dri/card1",
		"/dev/dri/card2",
		"/dev/dri/renderD128",
		"/dev/dri/renderD129",
	};

	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
		int fd = open(candidates[i], O_RDWR | O_CLOEXEC);
		if (fd < 0)
			continue;

		drmVersion *version = drmGetVersion(fd);
		if (!version) {
			close(fd);
			continue;
		}

		printf("util_open: %s, %.*s\n",
		       candidates[i],
		       version->name_len,
		       version->name ? version->name : "");

		if (version->name && strcmp(version->name, "virtio_gpu") == 0) {
			drmFreeVersion(version);
			return fd;
		}

		drmFreeVersion(version);
		close(fd);
	}

	return -1;
}

static const char *connector_status_str(unsigned int status)
{
	switch (status) {
	case DRM_MODE_CONNECTED:
		return "connected";
	case DRM_MODE_DISCONNECTED:
		return "disconnected";
	case DRM_MODE_UNKNOWNCONNECTION:
		return "unknown";
	default:
		return "invalid";
	}
}

static void dump_mode(const drmModeModeInfo *mode)
{
	printf("    mode=%s %ux%u clock=%u flags=0x%x type=0x%x\n",
	       mode->name,
	       mode->hdisplay,
	       mode->vdisplay,
	       mode->clock,
	       mode->flags,
	       mode->type);
}

static void list_resources(struct testapp_ctx *ctx)
{
	ctx->res = drmModeGetResources(ctx->fd);
	if (!ctx->res) {
		perror("drmModeGetResources");
		return;
	}

	if (drmSetClientCap(ctx->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0)
		perror("drmSetClientCap(DRM_CLIENT_CAP_UNIVERSAL_PLANES)");

	ctx->plane_res = drmModeGetPlaneResources(ctx->fd);
	if (!ctx->plane_res)
		perror("drmModeGetPlaneResources");

	printf("Connectors (%d)\n", ctx->res->count_connectors);
	for (int i = 0; i < ctx->res->count_connectors; i++) {
		drmModeConnector *conn = drmModeGetConnector(ctx->fd, ctx->res->connectors[i]);
		if (!conn)
			continue;

		printf("  connector[%d]=%u type=%s status=%s encoders=%d modes=%d\n",
		       i,
		       conn->connector_id,
		       drmModeGetConnectorTypeName(conn->connector_type),
		       connector_status_str(conn->connection),
		       conn->count_encoders,
		       conn->count_modes);

		for (int m = 0; m < conn->count_modes; m++)
			dump_mode(&conn->modes[m]);

		drmModeFreeConnector(conn);
	}

	printf("CRTCs (%d)\n", ctx->res->count_crtcs);
	for (int i = 0; i < ctx->res->count_crtcs; i++)
		printf("  crtc[%d]=%u\n", i, ctx->res->crtcs[i]);

	printf("Encoders (%d)\n", ctx->res->count_encoders);
	for (int i = 0; i < ctx->res->count_encoders; i++) {
		drmModeEncoder *enc = drmModeGetEncoder(ctx->fd, ctx->res->encoders[i]);
		if (!enc)
			continue;

		printf("  encoder[%d]=%u type=%u crtc_id=%u possible_crtcs=0x%x possible_clones=0x%x\n",
		       i,
		       enc->encoder_id,
		       enc->encoder_type,
		       enc->crtc_id,
		       enc->possible_crtcs,
		       enc->possible_clones);
		drmModeFreeEncoder(enc);
	}

	if (ctx->plane_res) {
		printf("Planes (%d)\n", ctx->plane_res->count_planes);
		for (int i = 0; i < ctx->plane_res->count_planes; i++) {
			drmModePlane *plane = drmModeGetPlane(ctx->fd, ctx->plane_res->planes[i]);
			if (!plane)
				continue;

			printf("  plane[%d]=%u crtc_id=%u fb_id=%u possible_crtcs=0x%x\n",
			       i,
			       plane->plane_id,
			       plane->crtc_id,
			       plane->fb_id,
			       plane->possible_crtcs);
			drmModeFreePlane(plane);
		}
	}
}

struct fb_state {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t size;
	uint32_t handle;
	uint32_t fb_id;
	void *map;
};

static void fb_cleanup(struct testapp_ctx *ctx, struct fb_state *fb)
{
	if (!fb)
		return;
	if (fb->map && fb->size)
		munmap(fb->map, fb->size);
	if (fb->fb_id)
		drmModeRmFB(ctx->fd, fb->fb_id);
	if (fb->handle) {
		struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
		ioctl(ctx->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
	}
}

static int create_dumb_fb(struct testapp_ctx *ctx, drmModeConnector *conn, drmModeCrtc *orig_crtc, struct fb_state *fb)
{
	memset(fb, 0, sizeof(*fb));

	const drmModeModeInfo *mode = &conn->modes[0];
	fb->width = mode->hdisplay;
	fb->height = mode->vdisplay;

	struct drm_mode_create_dumb create = {0};
	create.width = fb->width;
	create.height = fb->height;
	create.bpp = 32;

	if (ioctl(ctx->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
		perror("DRM_IOCTL_MODE_CREATE_DUMB");
		return -1;
	}

	fb->stride = create.pitch;
	fb->size = create.size;
	fb->handle = create.handle;

	uint32_t handles[4] = { fb->handle, 0, 0, 0 };
	uint32_t pitches[4] = { fb->stride, 0, 0, 0 };
	uint32_t offsets[4] = { 0, 0, 0, 0 };

	if (drmModeAddFB2(ctx->fd, fb->width, fb->height, DRM_FORMAT_XRGB8888,
			 handles, pitches, offsets, &fb->fb_id, 0) != 0) {
		perror("drmModeAddFB2");
		struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
		ioctl(ctx->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
		return -1;
	}

	struct drm_mode_map_dumb map = { .handle = fb->handle };
	if (ioctl(ctx->fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
		perror("DRM_IOCTL_MODE_MAP_DUMB");
		return -1;
	}

	fb->map = mmap(NULL, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, map.offset);
	if (fb->map == MAP_FAILED) {
		perror("mmap");
		fb->map = NULL;
		return -1;
	}

	memset(fb->map, 0x00, fb->size);
	uint32_t *pixels = fb->map;
	for (uint32_t y = 0; y < fb->height; y++) {
		for (uint32_t x = 0; x < fb->width; x++) {
			uint8_t r = (uint8_t)(x * 255 / (fb->width ? fb->width : 1));
			uint8_t g = (uint8_t)(y * 255 / (fb->height ? fb->height : 1));
			uint8_t b = 0x40;
			pixels[y * (fb->stride / 4) + x] = (r << 16) | (g << 8) | b;
		}
	}

	return 0;
}

static int simple_modeset(struct testapp_ctx *ctx)
{
	drmModeConnector *conn = NULL;
	drmModeEncoder *enc = NULL;
	drmModeCrtc *orig_crtc = NULL;
	struct fb_state fb;
	int ret = -1;

	ctx->res = drmModeGetResources(ctx->fd);
	if (!ctx->res) {
		perror("drmModeGetResources");
		return -1;
	}

	for (int i = 0; i < ctx->res->count_connectors; i++) {
		conn = drmModeGetConnector(ctx->fd, ctx->res->connectors[i]);
		if (conn && conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0)
			break;
		if (conn) {
			drmModeFreeConnector(conn);
			conn = NULL;
		}
	}

	if (!conn) {
		fprintf(stderr, "No connected connector with modes found\n");
		goto out;
	}

	if (conn->encoder_id)
		enc = drmModeGetEncoder(ctx->fd, conn->encoder_id);
	if (!enc && conn->count_encoders > 0)
		enc = drmModeGetEncoder(ctx->fd, conn->encoders[0]);
	if (!enc) {
		fprintf(stderr, "No encoder found for connector %u\n", conn->connector_id);
		goto out;
	}

	if (enc->crtc_id)
		orig_crtc = drmModeGetCrtc(ctx->fd, enc->crtc_id);

	if (create_dumb_fb(ctx, conn, orig_crtc, &fb) != 0)
		goto out;

	printf("modeset connector=%u encoder=%u crtc=%u fb=%u mode=%s\n",
	       conn->connector_id,
	       enc->encoder_id,
	       enc->crtc_id,
	       fb.fb_id,
	       conn->modes[0].name);

	if (drmModeSetCrtc(ctx->fd, enc->crtc_id, fb.fb_id, 0, 0,
			   &conn->connector_id, 1, &conn->modes[0]) != 0) {
		perror("drmModeSetCrtc");
		goto out;
	}

	printf("legacy modeset done\n");
	ret = 0;

out:
	if (orig_crtc)
		drmModeFreeCrtc(orig_crtc);
	if (enc)
		drmModeFreeEncoder(enc);
	if (conn)
		drmModeFreeConnector(conn);
	if (ctx->res) {
		drmModeFreeResources(ctx->res);
		ctx->res = NULL;
	}
	if (ret != 0)
		fb_cleanup(ctx, &fb);
	return ret;
}

int main(int argc, char **argv)
{
	bool do_a = false;
	bool do_b = false;
	int opt;
	struct testapp_ctx ctx = {0};
	int ret = EXIT_FAILURE;

	while ((opt = getopt(argc, argv, "ab")) != -1) {
		switch (opt) {
		case 'a':
			do_a = true;
			break;
		case 'b':
			do_b = true;
			break;
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (!do_a && !do_b) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	ctx.fd = open_virtio_gpu();
	if (ctx.fd < 0) {
		perror("open virtio_gpu DRM device");
		return EXIT_FAILURE;
	}

	if (do_a) {
		printf("[a] dump DRM resources\n");
		list_resources(&ctx);
	}

	if (do_b) {
		printf("[b] legacy modeset demo\n");
		if (simple_modeset(&ctx) != 0)
			goto out;
	}

	ret = EXIT_SUCCESS;

out:
	if (ctx.plane_res)
		drmModeFreePlaneResources(ctx.plane_res);
	if (ctx.res)
		drmModeFreeResources(ctx.res);
	if (ctx.fd >= 0)
		close(ctx.fd);
	return ret;
}
