--
-- xmonad example config file.
--
-- A template showing all available configuration hooks,
-- and how to override the defaults in your own xmonad.hs conf file.
--
-- Normally, you'd only override those defaults you care about.
--

import XMonad
import XMonad.ManageHook
import XMonad.Operations
import XMonad.Actions.CopyWindow
import XMonad.Actions.CycleWS
import XMonad.Actions.DwmPromote
import XMonad.Actions.RotSlaves
import XMonad.Actions.SinkAll
import XMonad.Hooks.ManageDocks
import XMonad.Hooks.DynamicLog
import XMonad.Hooks.SetWMName
import XMonad.Layout
import XMonad.Layout.Grid
import XMonad.Layout.NoBorders   ( noBorders, smartBorders )
import XMonad.Layout.Tabbed
import XMonad.Layout.ToggleLayouts
import XMonad.Layout.PerWorkspace
import XMonad.Layout.Master
import XMonad.Layout.ThreeColumns
import XMonad.Util.Run
import System.Exit

import qualified XMonad.StackSet as W
import qualified XMonad.Actions.FlexibleResize as Flex
import qualified Data.Map        as M
import Data.Bits ((.|.))
import Data.Ratio
import Graphics.X11
import System.IO

import XMonad.Layout.MouseResizableTile

-- The preferred terminal program, which is used in a binding below and by
-- certain contrib modules.
--
--myTerminal      = "terminal"
myTerminal      = "/home/rlblaster/.bin/spawn-terminal"

-- Width of the window border in pixels.
--
myBorderWidth   = 1

-- modMask lets you specify which modkey you want to use. The default
-- is mod1Mask ("left alt").  You may also consider using mod3Mask
-- ("right alt"), which does not conflict with emacs keybindings. The
-- "windows key" is usually mod4Mask.
--
myModMask       = mod4Mask

-- The default number of workspaces (virtual screens) and their names.
-- By default we use numeric strings, but any string may be used as a
-- workspace name. The number of workspaces is determined by the length
-- of this list.
--
-- A tagging example:
--
-- > workspaces = ["web", "irc", "code" ] ++ map show [4..9]
--
myWorkspaces    = [" 1 "," 2 "," 3 "," 4 "," 5 "," 6 "," 7 "," 8 "," 9 ", " web ", " em "]

-- Border colors for unfocused and focused windows, respectively.
--
myNormalBorderColor  = "#dddddd"
myFocusedBorderColor = "#ff0000"

-- Default offset of drawable screen boundaries from each physical
-- screen. Anything non-zero here will leave a gap of that many pixels
-- on the given edge, on the that screen. A useful gap at top of screen
-- for a menu bar (e.g. 15)
--
-- An example, to set a top gap on monitor 1, and a gap on the bottom of
-- monitor 2, you'd use a list of geometries like so:
--
-- > defaultGaps = [(18,0,0,0),(0,18,0,0)] -- 2 gaps on 2 monitors
--
-- Fields are: top, bottom, left, right.
--
-- myDefaultGaps   = [(16,16,0,0)]

------------------------------------------------------------------------
-- Key bindings. Add, modify or remove key bindings here.
--
myKeys conf@(XConfig {XMonad.modMask = modMask}) = M.fromList $

    -- launch a terminal
    [ ((modMask,               xK_Escape), spawn $ XMonad.terminal conf)
    , ((modMask .|. shiftMask, xK_Return), spawn $ XMonad.terminal conf)

    -- launch a floating terminal
    , ((modMask .|. controlMask, xK_Return), spawn "spawn-terminal -name XM-floating")

    -- launch dmenu
    --, ((modMask,               xK_p     ), spawn "exe=`dmenu_path | dmenu` && eval \"exec $exe\"")
    , ((modMask,               xK_p     ), spawn "gmrun")

    -- launch opera
    , ((modMask,               xK_o     ), spawn "opera")

    -- launch firefox
    , ((modMask,               xK_f     ), spawn "firefox")

    -- launch pidgin (gaim)
    , ((modMask,               xK_g     ), spawn "pidgin")

    -- refresh screen
    , ((modMask,               xK_r     ), spawn "xrefresh")

    -- add support for custom functions
    , ((modMask,               xK_F1    ), spawn "touch /dev/shm/wmfn1")
    , ((modMask,               xK_F2    ), spawn "touch /dev/shm/wmfn2")
    , ((modMask,               xK_F3    ), spawn "touch /dev/shm/wmfn3")
    , ((modMask,               xK_F4    ), spawn "touch /dev/shm/wmfn4")

    -- decrease volume
    , ((modMask,               xK_F11   ), spawn "amixer -c0 sset Master 5%- >/dev/null")

    -- increase volume
    , ((modMask,               xK_F12   ), spawn "amixer -c0 sset Master 5%+ >/dev/null")

    , ((modMask,               xK_backslash   ), spawn "/home/rlblaster/projects/latex-to-unicode/latex-to-unicode.sh")
    , ((modMask,               xK_equal   ), spawn "/home/rlblaster/projects/gp-gui/gp-gui.sh")

    -- close focused window 
    , ((modMask,               xK_c     ), kill)

    -- remove focused window from the current workspace
    , ((modMask .|. shiftMask,    xK_c     ), kill1) -- @@ Close the focused window


     -- Rotate through the available layout algorithms
    , ((modMask              , xK_space ), sendMessage NextLayout)

    --  Reset the layouts on the current workspace to default
    --, ((modMask .|. shiftMask, xK_space ), setLayout $ XMonad.layoutHook conf)

    -- Resize viewed windows to the correct size
    , ((modMask,               xK_n     ), refresh)

    -- Move focus to the next window
    , ((modMask,               xK_Tab   ), windows W.focusDown)

    -- Move focus to the next window
    , ((modMask,               xK_j     ), windows W.focusDown)

    -- Move focus to the previous window
    , ((modMask,               xK_k     ), windows W.focusUp  )

    -- Move focus to the master window
    , ((modMask,               xK_m     ), windows W.focusMaster  )

    -- Swap the focused window and the master window
    , ((modMask,               xK_Return), windows W.swapMaster)

    -- Swap the focused window with the next window
    , ((modMask .|. shiftMask, xK_j     ), windows W.swapDown  )

    -- Swap the focused window with the previous window
    , ((modMask .|. shiftMask, xK_k     ), windows W.swapUp    )

    -- Shrink the master area
    , ((modMask,               xK_h     ), sendMessage Shrink)

    -- Expand the master area
    , ((modMask,               xK_l     ), sendMessage Expand)

    -- Push window back into tiling
    , ((modMask,               xK_t     ), withFocused $ windows . W.sink)

    -- Increment the number of windows in the master area
    , ((modMask              , xK_comma ), sendMessage (IncMasterN 1))

    -- Deincrement the number of windows in the master area
    , ((modMask              , xK_period), sendMessage (IncMasterN (-1)))

    -- toggle the status bar gap
    -- , ((modMask              , xK_b     ),
    --      modifyGap (\i n -> let x = (XMonad.defaultGaps conf ++ repeat (0,0,0,0)) !! i
    --                         in if n == x then (0,0,0,0) else x))
    , ((modMask              , xK_b     ), sendMessage ToggleStruts)
    , ((modMask              , xK_s     ), sendMessage ToggleStruts)

    -- switch to previous workspace
    , ((modMask, xK_z), toggleWS)

    -- toggle to fullscreen.
    , ((modMask, xK_x), sendMessage ToggleLayout)

    -- cycle through non-empty workspaces
     , ((modMask, xK_Right), moveTo Next NonEmptyWS)
     , ((modMask, xK_Left), moveTo Prev NonEmptyWS)

    -- Quit xmonad
    {-, ((modMask .|. shiftMask, xK_q     ), io (exitWith ExitSuccess))-}

    -- Restart xmonad
    , ((modMask .|. controlMask, xK_q     ),
          broadcastMessage ReleaseResources >> restart "xmonad" True)

    -- Fix the Java bug
    --, ((modMask              , xK_a     ), setWMName "LG3D")
    , ((modMask              , xK_a     ), takeTopFocus)

    ]
    ++

    --
    -- mod-[1..9], Switch to workspace N
    -- mod-shift-[1..9], Move client to workspace N
    --
    [((m .|. modMask, k), windows $ f i)
        | (i, k) <- zip (XMonad.workspaces conf) ([xK_1 .. xK_9] ++ [xK_w, xK_e])
        {-, (f, m) <- [(W.greedyView, 0), (W.shift, shiftMask)]]-}
        , (f, m) <- [(W.view, 0), (W.shift, shiftMask), (copy, shiftMask .|. controlMask)]]


------------------------------------------------------------------------
-- Mouse bindings: default actions bound to mouse events
--
myMouseBindings (XConfig {XMonad.modMask = modMask}) = M.fromList $

    -- mod-button1, Set the window to floating mode and move by dragging
    [ ((modMask, button1), (\w -> focus w >> mouseMoveWindow w))

    -- mod-button2, Raise the window to the top of the stack
    , ((modMask, button2), (\w -> focus w >> windows W.swapMaster))

    -- mod-button3, Set the window to floating mode and resize by dragging
    , ((modMask, button3), (\w -> focus w >> mouseResizeWindow w))

    -- mouse scroll wheel, Cycle through workspaces
     , ((modMask, button4), (\_ -> moveTo Prev NonEmptyWS))
     , ((modMask, button5), (\_ -> moveTo Next NonEmptyWS))
    ]

------------------------------------------------------------------------
-- Layouts:

-- You can specify and transform your layouts by modifying these values.
-- If you change layout bindings be sure to use 'mod-shift-space' after
-- restarting (with 'mod-q') to reset your layout state to the new
-- defaults, as xmonad preserves your old layout settings by default.
--
-- The available layouts.  Note that each layout is separated by |||,
-- which denotes layout choice.
--

myLayout = avoidStruts $
           toggleLayouts(noBorders Full) $ smartBorders $
           --onWorkspace "3 web" (noBorders (tabbed shrinkText defaultTheme) ||| Grid ||| Mirror tiled ||| (mastered (3/100) (1/5) $ Tall 1 (3/100) (75/100)) ||| tiled) $
           onWorkspace "3 web" (noBorders (tabbed shrinkText defaultTheme) ||| Grid ||| Mirror tiled ||| tiled) $
           --onWorkspace "5 mm" (Mirror tiled ||| (mastered (3/100) (1/5) $ Tall 1 (3/100) (75/100)) ||| tiled ||| noBorders (tabbed shrinkText defaultTheme) ||| Grid) $
           --onWorkspace "6 misc" ((mastered (3/100) (1/5) $ Tall 1 (3/100) (75/100)) ||| tiled ||| noBorders (tabbed shrinkText defaultTheme) ||| Grid ||| Mirror tiled) $
           tiled ||| noBorders (tabbed shrinkText defaultTheme) ||| Grid ||| Mirror tiled ||| ThreeCol 1 (3/100) (1/2)
  where
     -- default tiling algorithm partitions the screen into two panes
     tiled   = Tall nmaster delta ratio

     -- The default number of windows in the master pane
     nmaster = 1

     -- Default proportion of screen occupied by master pane
     ratio   = 55/100

     -- Percent of screen to increment by when resizing panes
     delta   = 3/100

------------------------------------------------------------------------
-- Window rules:

-- Execute arbitrary actions and WindowSet manipulations when managing
-- a new window. You can use this to, for example, always float a
-- particular program, or have a client always appear on a particular
-- workspace.
--
-- To find the property name associated with a program, use
-- > xprop | grep WM_CLASS
-- and click on the client you're interested in.
--
-- To match on the WM_NAME, you can use 'title' in the same way that
-- 'className' and 'resource' are used below.
--
myManageHook = composeAll
    [ resource  =? "desktop_window"                      --> doIgnore
    , resource  =? "kdesktop"                            --> doIgnore
    , title =? "XM-floating"                             --> doFloat
    , title =? "XM-start-on-1"                           --> doF(W.shift " 1 ")
    , className =? "Gimp"                                --> doFloat
    --, className =? "MPlayer"                             --> doFloat
    , className =? "Vuze"                                --> doF(W.shift " 9 ")
    , className =? "Pidgin"                              --> doF(W.shift " em ")
    , stringProperty "WM_WINDOW_ROLE" =? "browser"       --> doF(W.shift " web ")
    ]


------------------------------------------------------------------------
-- Status bars and logging

-- dynamiclog pretty printer for dzen
--
myPP h = defaultPP 
                 { ppCurrent = wrap "^fg(#AAAAAA)^bg(#444444)^p(2)^ca(1,xdotool key super+" "^ca()^p(2)^fg()^bg()" . \wsId -> (wsId!!1) : ')' : (if (':' `elem` wsId) then drop 2 wsId else wsId)    -- Trim the '[Int]:' from workspace tags
                  , ppVisible = wrap "^bg(#000000)^fg(#AAAAAA)^p(2)" "^p(2)^fg()^bg()"
                  , ppHidden = wrap "^fg(#AAAAAA)^bg()^p(2)^ca(1,xdotool key super+" "^ca()^p(2)^fg()^bg()" . \wsId -> (wsId!!1) : ')' : (if (':' `elem` wsId) then drop 2 wsId else wsId)
                  , ppHiddenNoWindows = wrap "^fg(#444444)^bg(#000000)^p(2)^ca(1,xdotool key super+" "^ca()^p(2)^fg()^bg()" . id . \wsId -> (wsId!!1) : ')' : (if (':' `elem` wsId) then drop 2 wsId else wsId)
                  , ppSep     = "  ^fg(#777777)^r(2x2)^fg()  "
                  , ppWsSep           = "^fg(#444444)^r(1x16)^fg()"
                  --, ppLayout  = dzenColor "#777777" "" . (\x -> x)
                  , ppLayout  = wrap "^fg(#777777)^ca(1,xdotool key super+space)" "^ca()" . (\x -> x)
--                  , ppTitle   = dzenColor "white" "" . wrap "< " " >" 
                  , ppTitle   = dzenColor "#777777" ""
                  , ppOutput   = hPutStrLn h
                  }

------------------------------------------------------------------------
-- Now run xmonad with all the defaults we set up.

dzenFont = "-fn '-xos4-terminus-medium-r-normal--16-160-72-72-c-80-iso10646-1' ";
dzenColors = " -bg '#000000' -fg '#777777' "
dzenMiscOptions = " -h 17 -sa c -ta l -tw 1300 -e button3= "

dzenOptions = dzenColors ++ dzenFont ++ dzenMiscOptions
statusBarCmd = "/home/rlblaster/.xmonad/dzen/dzen2 " ++ dzenOptions

main = do din <- spawnPipe statusBarCmd
          xmonad $ defaultConfig 
                     {
                       -- simple stuff
                         terminal           = myTerminal,
                         borderWidth        = myBorderWidth,
                         modMask            = myModMask,
                         workspaces         = myWorkspaces,
                         normalBorderColor  = myNormalBorderColor,
                         focusedBorderColor = myFocusedBorderColor,
                         -- defaultGaps        = myDefaultGaps,

                       -- key bindings
                         keys               = myKeys,
                         mouseBindings      = myMouseBindings,

                       -- hooks, layouts
                         layoutHook         = myLayout,
                         manageHook         = myManageHook,
                         logHook            = dynamicLogWithPP $ myPP din,
                         startupHook        = setWMName "LG3D"
                     }

-- Workaround for buggy xmonad code (to make java applications focus correctly):

when              :: (Monad m) => Bool -> m () -> m ()
when p s          =  if p then s else return ()

-- Send WM_TAKE_FOCUS
takeTopFocus = withWindowSet $ maybe (setFocusX =<< asks theRoot) takeFocusX . W.peek

atom_WM_TAKE_FOCUS      = getAtom "WM_TAKE_FOCUS"
takeFocusX w = withWindowSet $ \ws -> do
    dpy <- asks display
    wmtakef <- atom_WM_TAKE_FOCUS
    wmprot <- atom_WM_PROTOCOLS

    protocols <- io $ getWMProtocols dpy w
    when (wmtakef `elem` protocols) $ do
        io $ allocaXEvent $ \ev -> do
            setEventType ev clientMessage
            setClientMessageEvent ev w wmprot 32 wmtakef currentTime
            sendEvent dpy w False noEventMask ev


