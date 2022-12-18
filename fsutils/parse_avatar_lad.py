#!/usr/bin/env python3

# open the avatar_lad.xml file and load it as a dom object

from xml.dom import minidom
import os
import sys
import argparse

parser = argparse.ArgumentParser()
# add positional argument for avatar_lad.xml
parser.add_argument("avatar_lad", help="path to avatar_lad.xml file")

args = parser.parse_args()


# using minidom open the avatar_lad.xml file and load it as a dom object


# open the avatar_lad.xml file and load it as a dom object

dom = minidom.parse(open(args.avatar_lad, "rb"))


# get the first element in the dom object

root = dom.documentElement

# find the "skeleton" child node
skeleton = root.getElementsByTagName("skeleton")[0]


for child in skeleton.childNodes:
    if child.nodeType == child.ELEMENT_NODE:
    # if tagname is "attachment_point" and 'hud' attribute doesn't exist print id and name
        if child.tagName == "attachment_point":
            if not child.hasAttribute("hud"):
                print(f"{child.getAttribute('id')} - {child.getAttribute('name')}")
            else:
                print(f"{child.getAttribute('id')} - [HUD] {child.getAttribute('name')}")


