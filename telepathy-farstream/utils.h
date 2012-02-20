#ifndef __UTILS_H__
#define __UTILS_H__

/*
 * tp_media_type_to_fs:
 * @type: A Telepathy Media Stream Type
 *
 * Converts a Telepathy Media Stream Type to the Farstream equivalent
 *
 * Return: A Farstream Stream Type
 */

static inline FsMediaType
tp_media_type_to_fs (TpMediaStreamType type)
{
  switch (type)
    {
    case TP_MEDIA_STREAM_TYPE_AUDIO:
      return FS_MEDIA_TYPE_AUDIO;
    case TP_MEDIA_STREAM_TYPE_VIDEO:
      return FS_MEDIA_TYPE_VIDEO;
    default:
      g_return_val_if_reached(0);
    }
}

static inline TpMediaStreamDirection
fsdirection_to_tpdirection (FsStreamDirection dir)
{
  switch (dir) {
  case FS_DIRECTION_NONE:
    return TP_MEDIA_STREAM_DIRECTION_NONE;
  case FS_DIRECTION_SEND:
    return TP_MEDIA_STREAM_DIRECTION_SEND;
  case FS_DIRECTION_RECV:
    return TP_MEDIA_STREAM_DIRECTION_RECEIVE;
  case FS_DIRECTION_BOTH:
    return TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL;
  default:
    g_assert_not_reached ();
  }
}

static inline FsStreamDirection
tpdirection_to_fsdirection (TpMediaStreamDirection dir)
{
  switch (dir) {
  case TP_MEDIA_STREAM_DIRECTION_NONE:
    return FS_DIRECTION_NONE;
  case TP_MEDIA_STREAM_DIRECTION_SEND:
    return FS_DIRECTION_SEND;
  case TP_MEDIA_STREAM_DIRECTION_RECEIVE:
    return FS_DIRECTION_RECV;
  case TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL:
    return FS_DIRECTION_BOTH;
  default:
    g_assert_not_reached ();
  }
}


#endif /* __UTILS_H__ */
