#ifndef __UTILS_H__
#define __UTILS_H__

/**
 * tp_media_type_to_fs:
 * @type: A Telepathy Media Stream Type
 *
 * Converts a Telepathy Media Stream Type to the Farsight2 equivalent
 *
 * Return: A Farsight2 Stream Type
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


#endif /* __UTILS_H__ */
