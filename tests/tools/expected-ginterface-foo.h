typedef struct _XyzBadgerIrritable XyzBadgerIrritable;
typedef struct _XyzBadgerIrritableInterface XyzBadgerIrritableInterface;

GType xyz_badger_irritable_get_type (void);

#define XYZ_BADGER_TYPE_IRRITABLE \
  (xyz_badger_irritable_get_type ())
#define XYZ_BADGER_IRRITABLE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), XYZ_BADGER_TYPE_IRRITABLE, \
                               XyzBadgerIrritable))
#define XYZ_BADGER_IS_IRRITABLE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XYZ_BADGER_TYPE_IRRITABLE))
#define XYZ_BADGER_IRRITABLE_GET_IFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), XYZ_BADGER_TYPE_IRRITABLE, \
                                  XyzBadgerIrritableInterface))
