-- This Applescript changes all of the icon labels on the disk image to
--  a different color.
-- Usage: osascript set-labels.applescript <volume-name>
-- where <volume-name> is the volume name of the disk image.

on run argv
	tell application "Finder"
		set diskname to item 1 of argv
		-- The magic happens here: This will set the label color of
		--  the supplied disk volume name to gray. Change the 7
		--  to change the color: 0 is no label, then red, orange,
		--  yellow, green, blue, purple, or gray.
		set label index of every file of disk diskname to 7
	end tell
end run