typedef struct _XyzBadgerMushroomSnake XyzBadgerMushroomSnake;
typedef struct _XyzBadgerMushroomSnakeClass XyzBadgerMushroomSnakeClass;
typedef struct _XyzBadgerMushroomSnakePrivate XyzBadgerMushroomSnakePrivate;

GType xyz_badger_mushroom_snake_get_type (void);

#define XYZ_BADGER_TYPE_MUSHROOM_SNAKE \
  (xyz_badger_mushroom_snake_get_type ())
#define XYZ_BADGER_MUSHROOM_SNAKE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), XYZ_BADGER_TYPE_MUSHROOM_SNAKE, \
                               XyzBadgerMushroomSnake))
#define XYZ_BADGER_IS_MUSHROOM_SNAKE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XYZ_BADGER_TYPE_MUSHROOM_SNAKE))
#define XYZ_BADGER_MUSHROOM_SNAKE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), XYZ_BADGER_TYPE_MUSHROOM_SNAKE, \
                              XyzBadgerMushroomSnakeClass))
