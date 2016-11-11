// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/math_util.h"

/// Describes the layout of the window framebuffer (size and top/bottom screen positions)
class FramebufferLayout {
public:
    /**
     * Factory method for constructing a default FramebufferLayout
     * @param width Window framebuffer width in pixels
     * @param height Window framebuffer height in pixels
     * @return Newly created FramebufferLayout object with default screen regions initialized
     */
    static FramebufferLayout DefaultFrameLayout(unsigned width, unsigned height);

    /**
     * Factory method for constructing a FramebufferLayout with only the top screen
     * @param width Window framebuffer width in pixels
     * @param height Window framebuffer height in pixels
     * @return Newly created FramebufferLayout object with default screen regions initialized
     */
    static FramebufferLayout SingleFrameLayout(unsigned width, unsigned height);

    /**
     * Factory method for constructing a Frame with the a 4x size Top screen with a 1x size bottom screen on the right
     * This is useful in particular because it matches well with a 1920x1080 resolution monitor
     * @param width Window framebuffer width in pixels
     * @param height Window framebuffer height in pixels
     * @return Newly created FramebufferLayout object with default screen regions initialized
     */
    static FramebufferLayout LargeFrameLayout(unsigned width, unsigned height);

    /**
     * Conditionally changes the layout to its inverse. For example, if a layout has a prominent top frame and a small bottom frame,
     * this function should switch them so the bottom frame is now large.
     * @param is_reverse If this is true, and the layout is a reverse layout, then nothing will change. This is a toggle
     */
    void ReverseFrames(bool is_reverse);

    FramebufferLayout();
    FramebufferLayout& operator= (const FramebufferLayout &cSource);

    unsigned width;
    unsigned height;
    bool top_screen_enabled;
    bool bottom_screen_enabled;
    MathUtil::Rectangle<unsigned> top_screen;
    MathUtil::Rectangle<unsigned> bottom_screen;
private:

    typedef FramebufferLayout (*InverseLayout)(unsigned, unsigned);

    static FramebufferLayout InverseDefaultLayout(unsigned width, unsigned height);
    static FramebufferLayout InverseSingleLayout(unsigned width, unsigned height);
    static FramebufferLayout InverseLargeLayout(unsigned width, unsigned height);

    FramebufferLayout(unsigned width, unsigned height, bool tenabled, bool benabled, InverseLayout,
        MathUtil::Rectangle<unsigned> tscreen, MathUtil::Rectangle<unsigned> bscreen);
    InverseLayout inverse;
    bool is_reverse_layout;
};
