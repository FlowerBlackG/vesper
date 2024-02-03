# 落霞合成器

## 简介

落霞是一款基于 wlroots 的 Wayland 合成器与窗口管理器，支持用 VNC 协议将画面传送到远程桌面。可作为虚拟实验平台的组件。

该项目为作者毕业设计项目的一部分。

## 整体结构

![img](./doc/images/vesper-overview.drawio.svg)

## 项目进展

**desktop - server**

* [ ] ?

---

**desktop - scene**

* [X] wlr_scene_create
* [X] wlr_scene_attach_output_layout
* [ ] wlr_scene_xdg_surface_create
* [ ] wlr_scene_output_create
* [ ] wlr_scene_output_layout_add_output
* [ ] wlr_scene_output_commit
* [ ] wlr_scene_get_scene_output
* [ ] wlr_scene_node_set_position
* [ ] wlr_scene_output_send_frame_done
* [ ] wlr_scene_node_destroy
* [ ] wlr_scene_surface_try_from_buffer
* [ ] wlr_scene_node_raise_to_top
* [ ] wlr_scene_buffer_from_node

---

**vnc**

* [ ] ?

---

**control**

* [ ] ?

## 参考

* \[1\] Drew DeVault. The Wayland Protocol. [https://wayland-book.com/](https://wayland-book.com/)
* \[2\] Kristian Hogsberg. The Wayland Protocol. 2012. [https://wayland.freedesktop.org/docs/html/](https://wayland.freedesktop.org/docs/html/)
* \[3\] T. Richardson & J. Levine. The Remote Framebuffer Protocol. [https://datatracker.ietf.org/doc/html/rfc6143](https://datatracker.ietf.org/doc/html/rfc6143)
* \[4\] Introduction to wlroots. [https://way-cooler.org/book/wlroots_introduction.html](https://way-cooler.org/book/wlroots_introduction.html)
* \[5\] wlroots – A modular Wayland compositor library. [https://gitlab.freedesktop.org/wlroots/wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
* \[6\] swaywm. sway – i3-compatible Wayland compositor. [https://github.com/swaywm/sway](https://github.com/swaywm/sway)
* \[7\] any1. wayvnc – A VNC server for wlroots based Wayland compositors. [https://github.com/any1/wayvnc](https://github.com/any1/wayvnc)
* \[8\] pixman – Image processing and manipulation library. [https://gitlab.freedesktop.org/pixman/pixman](https://gitlab.freedesktop.org/pixman/pixman)
* \[9\] wlroots. tinywl. [https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/tinywl](https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/tinywl)
* \[10\] Wayland. weston – A lightweight and functional Wayland compositor. [https://gitlab.freedesktop.org/wayland/weston](https://gitlab.freedesktop.org/wayland/weston)
