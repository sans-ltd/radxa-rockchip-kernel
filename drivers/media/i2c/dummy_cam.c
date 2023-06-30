#include <linux/compat.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rk-camera-module.h>
#include <linux/version.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-mediabus.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Mayer, SANS Software and Network Solutions Limited");
MODULE_DESCRIPTION("This module simulates a camera.");
MODULE_VERSION("0.01");

#define DUMMY_CAM_NAME		"dummy_cam"
#define DRIVER_VERSION		KERNEL_VERSION(0, 0x01, 0x1)

struct dummy_cam {
	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct clk *clk;
	struct v4l2_rect crop_rect;
	

	struct v4l2_ctrl *hblank_ctrl;
	struct v4l2_ctrl *vblank_ctrl;
	struct v4l2_ctrl *pixel_rate_ctrl;
	const struct dummy_cam_mode *cur_mode;

	// Rockchip Meta Data from Device Tree
	u32 module_index;
	const char *module_facing;
	const char *module_name;
	const char *len_name;

	u32 height;
	u32 width;
	u32 pixel_format;
	struct v4l2_fract max_fps;

	s32 h_blank;
	s32 v_blank;
	s32 pixel_rate;
	s32 link_freq;
};

static s64 link_freq_menu_items[] = {
	0
};

static struct dummy_cam *to_dummy_cam(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct dummy_cam, subdev);
}

/* V4L2 subdev video operations */
// 
static int dummy_cam_s_stream(struct v4l2_subdev *sd, int enable)
{
	// struct i2c_client *client = v4l2_get_subdevdata(sd);
	// struct dummy_cam *priv = to_dummy_cam(client);

    return 0;
}

static int dummy_cam_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	// struct i2c_client *client = v4l2_get_subdevdata(sd);
	// struct dummy_cam *priv = to_dummy_cam(client);

    // Pass max. FPS here.
	// const struct dummy_cam_mode *mode = priv->cur_mode;
	// fi->interval = mode->max_fps;

	return 0;
}

static int dummy_cam_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
    // Read Only 

	return 0;
}

static int dummy_cam_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct dummy_cam *priv = to_dummy_cam(client);
	// const struct dummy_cam_mode *mode = priv->cur_mode;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	fmt->format.width = priv->width;  // TODO make values configurable.
	fmt->format.height = priv->height;
	fmt->format.code = priv->pixel_format;
	fmt->format.field = V4L2_FIELD_NONE;

	return 0;
}

static int dummy_cam_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct dummy_cam *priv = to_dummy_cam(client);	
	
	if (code->index != 0)
		return -EINVAL;
	code->code = priv->pixel_format;  // TODO make it configurable.

	return 0;
}

// Used to get the supported modes ? 
static int dummy_cam_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_pad_config *cfg,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct dummy_cam *priv = to_dummy_cam(client);

	if (fie->index >= 1)  // We support only one mode.
		return -EINVAL;

	if (fie->code != priv->pixel_format)  // TODO Make it configurable.
		return -EINVAL;

	fie->width = priv->width;
	fie->height = priv->height;
	fie->interval.numerator = priv->max_fps.numerator;  
    fie->interval.denominator = priv->max_fps.denominator;

	return 0;
}

static void dummy_cam_get_module_inf(struct dummy_cam *dummy_cam,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strlcpy(inf->base.sensor, DUMMY_CAM_NAME, sizeof(inf->base.sensor));
	strlcpy(inf->base.module, dummy_cam->module_name,
		sizeof(inf->base.module));
	strlcpy(inf->base.lens, dummy_cam->len_name, sizeof(inf->base.lens));
}

static long dummy_cam_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct dummy_cam *dummy_cam = to_dummy_cam(client);
	long ret = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		dummy_cam_get_module_inf(dummy_cam, (struct rkmodule_inf *)arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long dummy_cam_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	long ret;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = dummy_cam_ioctl(sd, cmd, inf);
		if (!ret)
			ret = copy_to_user(up, inf, sizeof(*inf));
		kfree(inf);
		break;
	case RKMODULE_AWB_CFG:
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(cfg, up, sizeof(*cfg));
		if (!ret)
			ret = dummy_cam_ioctl(sd, cmd, cfg);
		kfree(cfg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

/* Various V4L2 operations tables */
static struct v4l2_subdev_video_ops dummy_cam_subdev_video_ops = {
	// used to notify the driver that a video stream will start or has stopped.
    .s_stream = dummy_cam_s_stream,
	.g_frame_interval = dummy_cam_g_frame_interval,
};

// Special IOCTL needed for Rockchip.
static struct v4l2_subdev_core_ops dummy_cam_subdev_core_ops = {
	.ioctl = dummy_cam_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = dummy_cam_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_pad_ops dummy_cam_subdev_pad_ops = {
	.enum_mbus_code = dummy_cam_enum_mbus_code,
	.enum_frame_interval = dummy_cam_enum_frame_interval,
	.set_fmt = dummy_cam_set_fmt,
	.get_fmt = dummy_cam_get_fmt,
};

static struct v4l2_subdev_ops dummy_cam_subdev_ops = {
	.core = &dummy_cam_subdev_core_ops,
	.video = &dummy_cam_subdev_video_ops,
	.pad = &dummy_cam_subdev_pad_ops,
};

static int dummy_cam_ctrls_init(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct dummy_cam *priv = to_dummy_cam(client);
	// const struct dummy_cam_mode *mode = priv->cur_mode;
	s32 pixel_rate, h_blank, v_blank;
	int ret;
	// u32 fps = 0;

	v4l2_ctrl_handler_init(&priv->ctrl_handler, 10);

	/* blank */
	h_blank = priv->h_blank;
	priv->hblank_ctrl = v4l2_ctrl_new_std(&priv->ctrl_handler, NULL, V4L2_CID_HBLANK,
			  0, h_blank, 1, h_blank);

	v_blank = priv->v_blank;  
	priv->vblank_ctrl = v4l2_ctrl_new_std(&priv->ctrl_handler, NULL, V4L2_CID_VBLANK,
			  0, v_blank, 1, v_blank);

	/* freq */
	link_freq_menu_items[0] = priv->link_freq;
	v4l2_ctrl_new_int_menu(&priv->ctrl_handler, NULL, V4L2_CID_LINK_FREQ,
			       0, 0, link_freq_menu_items);

	pixel_rate = priv->pixel_rate; // TODO add real value
	priv->pixel_rate_ctrl = v4l2_ctrl_new_std(&priv->ctrl_handler, NULL, V4L2_CID_PIXEL_RATE,
			  0, pixel_rate, 1, pixel_rate);


	priv->subdev.ctrl_handler = &priv->ctrl_handler;
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls\n",
			priv->ctrl_handler.error);
		ret = priv->ctrl_handler.error;
		goto error;
	}

	ret = v4l2_ctrl_handler_setup(&priv->ctrl_handler);
	if (ret < 0) {
		dev_err(&client->dev, "Error %d setting default controls\n",
			ret);
		goto error;
	}

	return 0;
error:
	v4l2_ctrl_handler_free(&priv->ctrl_handler);
	return ret;
}



static int dummy_cam_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct dummy_cam *priv;
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	priv = devm_kzalloc(&client->dev, sizeof(struct dummy_cam), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &priv->module_index);
	dev_info(dev, "rockchip,camera-module-index:     %d", priv->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &priv->module_facing);
	dev_info(dev, "rockchip,camera-module-facing:    %s", priv->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &priv->module_name);
	dev_info(dev, "rockchip,camera-module-name:      %s", priv->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &priv->len_name);
	dev_info(dev, "rockchip,camera-module-lens-name: %s", priv->len_name);

	ret |= of_property_read_u32(node, "dummy_cam,width_px",
				       &priv->width);
	dev_info(dev, "dummy_cam,width_px:               %d", priv->width);
	ret |= of_property_read_u32(node, "dummy_cam,height_px",
				       &priv->height);
	dev_info(dev, "dummy_cam,height_px:              %d", priv->height);
		ret |= of_property_read_u32(node, "dummy_cam,pixel_format",
				       &priv->pixel_format);
	dev_info(dev, "dummy_cam,pixel_format:           0x%x", priv->pixel_format);
	ret |= of_property_read_u32(node, "dummy_cam,max_fps_numerator",
				       &priv->max_fps.numerator);
	dev_info(dev, "dummy_cam,max_fps_numerator:      %d", priv->max_fps.numerator);
	ret |= of_property_read_u32(node, "dummy_cam,max_fps_denominator",
				       &priv->max_fps.denominator);
	dev_info(dev, "dummy_cam,max_fps_denominator:    %d", priv->max_fps.denominator);
	ret |= of_property_read_s32(node, "dummy_cam,h_blank_px",
				       &priv->h_blank);
	dev_info(dev, "dummy_cam,h_blank_px:             %d", priv->h_blank);
	ret |= of_property_read_s32(node, "dummy_cam,v_blank_lines",
				       &priv->v_blank);
	dev_info(dev, "dummy_cam,v_blank_lines:          %d", priv->v_blank);
	ret |= of_property_read_s32(node, "dummy_cam,link_freq_hz",
				       &priv->link_freq);
	dev_info(dev, "dummy_cam,link_freq_hz:           %d", priv->link_freq);
	ret |= of_property_read_s32(node, "dummy_cam,pixel_rate_hz",
				       &priv->pixel_rate);
	dev_info(dev, "dummy_cam,pixel_rate_hz:          %d", priv->pixel_rate);

	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

    /*
	priv->clk = devm_clk_get(&client->dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_info(&client->dev, "Error %ld getting clock\n",
			 PTR_ERR(priv->clk));
		return -EPROBE_DEFER;
	}
    */

	v4l2_i2c_subdev_init(&priv->subdev, client, &dummy_cam_subdev_ops);
	ret = dummy_cam_ctrls_init(&priv->subdev);
	if (ret < 0)
		return ret;

	priv->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;

	priv->pad.flags = MEDIA_PAD_FL_SOURCE;
	priv->subdev.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&priv->subdev.entity, 1, &priv->pad);
	if (ret < 0)
		return ret;

	sd = &priv->subdev;
	memset(facing, 0, sizeof(facing));
	if (strcmp(priv->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 priv->module_index, facing,
		 DUMMY_CAM_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor_common(sd);
	if (ret < 0)
		return ret;

    dev_info(dev, "init done!");

	return ret;
}

static int dummy_cam_remove(struct i2c_client *client)
{
	struct dummy_cam *priv = to_dummy_cam(client);

	v4l2_async_unregister_subdev(&priv->subdev);
	media_entity_cleanup(&priv->subdev.entity);
	v4l2_ctrl_handler_free(&priv->ctrl_handler);

	return 0;
}

static const struct i2c_device_id dummy_cam_id[] = {
	{"dummy_cam", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, dummy_cam_id);

static const struct of_device_id dummy_cam_of_match[] = {
	{ .compatible = "sans_ltd,dummy_cam" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, dummy_cam_of_match);


static struct i2c_driver dummy_cam_i2c_driver = {
	.driver = {
		.of_match_table = of_match_ptr(dummy_cam_of_match),
		.name = "dummy_cam",
	},
	.probe = dummy_cam_probe,
	.remove = dummy_cam_remove,
	.id_table = dummy_cam_id,
};

module_i2c_driver(dummy_cam_i2c_driver);