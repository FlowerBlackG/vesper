# wlr_scene

wlr_scene 是 wlroots 默认的窗口管理器，以树形结构管理所有窗口。

## 场景树

### wlr_scene_tree

```c
/** A sub-tree in the scene-graph. */
struct wlr_scene_tree {
    struct wlr_scene_node node;
    struct wl_list children; // wlr_scene_node.link
};
```

树节点由 wlr_scene_tree 定义。每个 tree 表示的是一个“子树”的概念。每个 tree 节点包含一个 node 对象，用于存储节点信息。同时，包含一个 children 列表，用来表示所有的子树。可以发现，场景树是一个多叉树结构。

每个 wlr_scene 会保存一个树根节点。每个正常的窗口（指一般窗口。菜单那种弹出的浮动窗口 (popup) 就不是一般窗口）都会直接成为树根的孩子。

每个 popup 的上级节点都是“生出这个popup”的节点。例如，konsole 的一级汉堡菜单 popup 的上级是 konsole 本身，二级汉堡菜单的上级是一级汉堡菜单，三级汉堡菜单的上级是二级汉堡菜单。

正常窗口的上级是我们手动规定的，因为它默认就是顶层。popup 的上级是 xdg shell 自己定义的，我们拿来用就行。

### wlr_scene_node_type

```c
enum wlr_scene_node_type {
    WLR_SCENE_NODE_TREE,
    WLR_SCENE_NODE_RECT,
    WLR_SCENE_NODE_BUFFER,
};
```

每个 node 都具有一个类型。

TREE 表示该 node 仅用于记录节点基本信息，如节点间的父母关系。

BUFER 表示该节点用于保存图像缓冲信息。

### 场景树模型

每个 scene 内部都有一棵场景树。树的节点可以是 buffer、tree 或 rect。其中，树根一定是 tree，所有非叶节点也必须是 tree。buffer 和 rect 可以是叶节点，而他们的上级节点类型一定是 tree。

## scene_damage_outputs

将所有 damage 合并起来。如果 damage 成功被合并（即确实需要更新画面），就进行如下顺序操作：

```
scene_damage_outputs
  -> wlr_output_schedule_frame
    -> schedule_frame_handle_idle_timer
      -> output.frame.event
```


