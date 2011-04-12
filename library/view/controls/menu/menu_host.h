
#ifndef __view_menu_host_h__
#define __view_menu_host_h__

#pragma once

namespace view
{

    class SubmenuView;
    class View;

    // SubmenuView uses a MenuHost to house the SubmenuView. MenuHost typically
    // extends the native Widget type, but is defined here for clarity of what
    // methods SubmenuView uses.
    //
    // SubmenuView owns the MenuHost. When SubmenuView is done with the MenuHost
    // |DestroyMenuHost| is invoked. The one exception to this is if the native
    // OS destroys the widget out from under us, in which case |MenuHostDestroyed|
    // is invoked back on the SubmenuView and the SubmenuView then drops references
    // to the MenuHost.
    class MenuHost
    {
    public:
        // Creates the platform specific MenuHost. Ownership passes to the caller.
        static MenuHost* Create(SubmenuView* submenu_view);

        // Initializes and shows the MenuHost.
        virtual void InitMenuHost(HWND parent,
            const gfx::Rect& bounds,
            View* contents_view,
            bool do_capture) = 0;

        // Returns true if the menu host is visible.
        virtual bool IsMenuHostVisible() = 0;

        // Shows the menu host. If |do_capture| is true the menu host should do a
        // mouse grab.
        virtual void ShowMenuHost(bool do_capture) = 0;

        // Hides the menu host.
        virtual void HideMenuHost() = 0;

        // Destroys and deletes the menu host.
        virtual void DestroyMenuHost() = 0;

        // Sets the bounds of the menu host.
        virtual void SetMenuHostBounds(const gfx::Rect& bounds) = 0;

        // Releases a mouse grab installed by |ShowMenuHost|.
        virtual void ReleaseMenuHostCapture() = 0;

        // Returns the native window of the MenuHost.
        virtual HWND GetMenuHostWindow() = 0;

    protected:
        virtual ~MenuHost() {}
    };

} //namespace view

#endif //__view_menu_host_h__