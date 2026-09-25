# Controller support — 1.4.0

## Set up your controller

1. Open **Settings > Controls** and assign your controller to the intended player.
2. Customize that player's controls. For an N64-style controller, check the
   gameplay **B** action (heavy brake / back in original game menus) and the
   overlay **Back (Menu)** action separately. Bind both as desired; recognizing
   physical B in the scanner does not automatically bind both actions.
3. Map **Eject from Bike** and **Spoke Jam Attack** to available buttons. The
   default stick-click bindings may not suit an N64-style controller.

The common defaults are designed for a modern dual-stick controller. N64 C
buttons can appear as right-stick directions during remapping; this is expected.
Bindings for opposite C directions share axes, so this does not provide separate
simultaneous opposite-button input. Existing layouts and saved bindings are kept.

## 8BitDo 64 Bluetooth Controller

The 1.3.1 Revision 2 mapping improvements are retained. The Linux reporter
confirmed that their **8BitDo 64 Bluetooth Controller, Model 8ONE**, could
configure the buttons except the left-stick click in their tested setup.
D-mode mappings and the separate Switch-mode mappings remain supported.
Other models, firmware versions, receivers and connection modes may expose
different devices; this is not a claim that every combination has been tested.

**Linux L3 limitation:** the tested Linux Bluetooth/evdev N64 driver layout
does not expose a separate thumb-click input. Remapping cannot recover an event
that the driver does not provide. This limitation applies to that driver/device
path, not all Linux controllers or all 8BitDo modes. Use another available button
for eject. No driver replacement or universal L3 repair is included.

## Assigned-profile correction in 1.4

Gameplay buttons, steering, dedicated actions and overlay input now follow the
assigned player's profile consistently. This repairs a source-level mismatch
where single-player gameplay could read an older default after another profile
was edited. Virtual-controller checks passed on Windows and Linux; the separate
PikaOS report about B not working in game menus still needs hardware confirmation.

Edited bindings are saved. A separately chosen custom layout may need to be
selected again after reconnecting or restarting; persisting that layout-to-device
association is a separate existing limitation. Editing the device's default
assigned layout does persist.

## If a button still does not work

Report the exact game build, controller model, D/S mode, connection method,
operating system and the action being configured. Include whether the button is
recognized in Controls and whether it fails in the overlay, original menus or race.

Optional diagnostics are off by default. `RR64_DIAGNOSTICS=1` records device
identity/mapping; `RR64_CONTROLLER_BUTTON_TRACE=1` records up to 256 raw/mapped
joystick button events. Press the failing button early, followed by working
buttons. This button trace does not collect keyboard or continuous-axis input.
Share the relevant log after reviewing it for private information; no ROM or save
is needed for a controller report.
