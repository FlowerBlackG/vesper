// SPDX-License-Identifier: MulanPSL-2.0

/*
 * 鼠标
 * 
 * 创建于 2024年2月10日 北京市海淀区
 */

#pragma once

#include "../../utils/wlroots-cpp.h"
#include "../../utils/ObjUtils.h"
#include "../../log/Log.h"
#include "../../bindings/pixman.h"

namespace vesper::desktop::server {

class View;
class Server;


class Cursor {

public:
    struct CreateOptions {
        Server* server;
        wlr_output_layout* wlrOutputLayout;
        const char* name;
        uint32_t size;
    };

    static Cursor* create(const CreateOptions&);

    ~Cursor();


    void processMotion(uint32_t timeMsec);
    
    void processMove(uint32_t timeMsec);
    void processResize(uint32_t timeMsec);

    void buttonEventHandler(uint32_t timeMsec, uint32_t button, wl_pointer_button_state state);


protected:
    Cursor() {}
    int init(const CreateOptions&);
    VESPER_OBJ_UTILS_DISABLE_COPY(Cursor);

public:

    enum class Mode {
        PASSTHROUGH,
        MOVE,
        RESIZE
    };

    Server* server = nullptr;
    wlr_cursor* wlrCursor = nullptr;
    wlr_xcursor_manager* wlrXCursorMgr = nullptr;
    Cursor::Mode cursorMode = Mode::PASSTHROUGH;

    View* grabbedView = nullptr;
    double grabX;
    double grabY;
    wlr_box grabGeoBox;

    uint32_t resizeEdges = 0;

    struct {
        wl_listener cursorMotion;
        wl_listener cursorMotionAbsolute;
        wl_listener cursorButton;
        wl_listener cursorAxis;
        wl_listener cursorFrame;
    } eventListeners;

};

} // namespace vesper::desktop::server
