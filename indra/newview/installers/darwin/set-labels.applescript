-- This Applescript changes all of the icon labels on the disk image to
--  a different color.
-- Usage: osascript set-labels.applescript <volume-name>
-- where <volume-name> is the volume name of the disk image.

on run argv
	tell application "Finder"
		set diskname to item 1 of argv
		tell disk diskname
			set file_list to every file
			repeat with i in file_list
				if the name of i is "Applications" then
					set the position of i to {368, 135}
				else if the name of i ends with ".app" then
					set the position of i to {134, 135}
				end if
			-- The magic happens here: This will set the label color of
			--  the file icon. Change the 7 to change the color: 0 is no
			--  label, then red, orange, yellow, green, blue, purple, or gray.
				set the label index of i to 7
			end repeat
			update without registering applications
			delay 4
		end tell
	end tell
end run
