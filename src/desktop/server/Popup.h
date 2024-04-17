// SPDX-License-Identifier: MulanPSL-2.0

/*
 * XDG Popup
 * 
 * 创建于 2024年4月15日 上海市嘉定区安亭镇
 */

#pragma once

#include "../../utils/wlroots-cpp.h"
#include "../../utils/ObjUtils.h"


namespace vesper::desktop::scene {
class SceneTreeNode;
}


namespace vesper::desktop::server {

class Server;


class Popup {

public:
    struct CreateOptions {
        Server* server;
        wlr_xdg_popup* xdgPopup;
    };

    static Popup* create(const CreateOptions&);

    ~Popup();
protected:
    Popup() {}
    VESPER_OBJ_UTILS_DISABLE_COPY(Popup);
    int init(const CreateOptions&);


public:
    Server* server;
    wlr_xdg_popup* wlrXdgPopup;


    struct {
        wl_listener commit;
        wl_listener destroy;
    } eventListeners = {0};

};

}

