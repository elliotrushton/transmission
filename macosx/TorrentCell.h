/******************************************************************************
 * Copyright (c) 2006-2012 Transmission authors and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#import <AppKit/AppKit.h>

@interface TorrentCell : NSActionCell
{
    NSUserDefaults* fDefaults;

    NSMutableDictionary* fTitleAttributes;
    NSMutableDictionary* fStatusAttributes;

    BOOL fTracking;
    BOOL fMouseDownControlButton;
    BOOL fMouseDownRevealButton;
    BOOL fMouseDownActionButton;
    BOOL fHover;
    BOOL fHoverControl;
    BOOL fHoverReveal;
    BOOL fHoverAction;

    NSColor* fBarBorderColor;
    NSColor* fBluePieceColor;
    NSColor* fBarMinimalBorderColor;
}

- (NSRect)iconRectForBounds:(NSRect)bounds;

- (void)addTrackingAreasForView:(NSView*)controlView
                         inRect:(NSRect)cellFrame
                   withUserInfo:(NSDictionary*)userInfo
                  mouseLocation:(NSPoint)mouseLocation;
- (void)setHover:(BOOL)hover;
- (void)setControlHover:(BOOL)hover;
- (void)setRevealHover:(BOOL)hover;
- (void)setActionHover:(BOOL)hover;
- (void)setActionPushed:(BOOL)pushed;

@end
