-- This Applescript sets up the installer disk image: size, background image,
--   icon view, background image, icon positioning.
-- Usage: osascript installer-dmg.applescript <volume-name>
-- where <volume-name> is the volume name of the disk image.

on run argv
	tell application "Finder"
		set diskname to item 1 of argv
		tell disk diskname
			open
			set bounds of container window to {400, 100, 900, 699}
			-- bit of a hack, window size isn't written until there is some user interaction
			-- double click the zoom button to trick Finder into saving it! [FS:CR]
			tell application "System Events" to tell process "Finder" to click button 2 of window 1
			tell application "System Events" to tell process "Finder" to click button 2 of window 1
			set file_list to every file
			repeat with i in file_list
				if the name of i is "Applications" then
					set the position of i to {391, 165}
				else if the name of i ends with ".app" then
					set the position of i to {121, 165}
				else if the name of i is "LGPL-License.txt" then
					set the position of i to {391, 265}
				else if the name of i is "VivoxAUP.txt" then
					set the position of i to {121, 265}
				end if
				-- Change the 7 to change the color: 0 is no label, then red,
				-- orange, yellow, green, blue, purple, or gray.
				set the label index of i to 7
			end repeat
			set theViewOptions to the icon view options of container window
			set background picture of theViewOptions to file "background.png"
			set arrangement of theViewOptions to not arranged
			set icon size of theViewOptions to 100
			set current view of container window to icon view
			set toolbar visible of container window to false
			set statusbar visible of container window to false
			update without registering applications
			delay 4
		end tell
	end tell
end run
