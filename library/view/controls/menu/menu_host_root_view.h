
#ifndef __view_menu_host_root_view_h__
#define __view_menu_host_root_view_h__

#pragma once

#include "../../view/root_view.h"

namespace view
{

    class MenuController;
    class SubmenuView;

    // MenuHostRootView is the RootView of the window showing the menu.
    // SubmenuView's scroll view is added as a child of MenuHostRootView.
    // MenuHostRootView forwards relevant events to the MenuController.
    //
    // As all the menu items are owned by the root menu item, care must be taken
    // such that when MenuHostRootView is deleted it doesn't delete the menu items.
    class MenuHostRootView : public RootView
    {
    public:
        MenuHostRootView(Widget* widget, SubmenuView* submenu);

        // When invoked subsequent events are NOT forwarded to the MenuController.
        void suspend_events() { suspend_events_ = true; }

        // Overridden from View:
        virtual bool OnMousePressed(const MouseEvent& event);
        virtual bool OnMouseDragged(const MouseEvent& event);
        virtual void OnMouseReleased(const MouseEvent& event, bool canceled);
        virtual void OnMouseMoved(const MouseEvent& event);
        virtual void OnMouseExited(const MouseEvent& event);
        virtual bool OnMouseWheel(const MouseWheelEvent& event);

    private:
        // Returns the MenuController for this MenuHostRootView.
        MenuController* GetMenuController();

        // The SubmenuView we contain.
        SubmenuView* submenu_;

        // Whether mouse dragged/released should be forwarded to the MenuController.
        bool forward_drag_to_menu_controller_;

        // Whether events are suspended. If true, no events are forwarded to the
        // MenuController.
        bool suspend_events_;

        DISALLOW_COPY_AND_ASSIGN(MenuHostRootView);
    };

} //namespace view

#endif //__view_menu_host_root_view_h__