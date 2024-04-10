# pixman 数据结构

## pixman_region32

```c
struct pixman_region32_data {
    long		size;
    long		numRects;
/*  pixman_box32_t	rects[size];   in memory but not explicitly declared */
};

struct pixman_box32
{
    int32_t x1, y1, x2, y2;
};

struct pixman_region32
{
    pixman_box32_t          extents;
    pixman_region32_data_t  *data;
};
```

region 内包含一个 box 列表。

## pixman_box

```c
struct pixman_box32
{
    int32_t x1, y1, x2, y2;
};
```

表示一段区域。区域左上角坐标是 (x1, y1)，右下角坐标是 (x2, y2)。
