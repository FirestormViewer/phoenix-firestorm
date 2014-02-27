from os import  walk
from os.path import join, dirname
import sys

import md5

from locale import getdefaultlocale

def asUTF8( aString ):
    sLocale = "iso-8859-15"
    if getdefaultlocale()[1] != None:
	    sLocale = getdefaultlocale()[1]

    return aString.decode( sLocale ).encode( "UTF-8" )

from msilib import FCICreate
totalFiles = 0

hHashes = {}

cabNames = []
linkNames = []

searchPath = sys.argv[1].replace( "/", "\\" )

if searchPath[ len( searchPath ) -1 ] != "\\":
    searchPath += "\\" 

for root, dirs, files in walk( sys.argv[1] ):
    totalFiles += len( files )
    for curFile in files:
        fullPath = join( root, curFile )
        strippedPath = fullPath[ len( searchPath ): ]
        data = open( fullPath, "rb" ).read()
        md5Hash = md5.new(data).hexdigest()
        writeFile = True
        if hHashes.has_key( md5Hash ):
            hHashes[ md5Hash ][0] += 1
            writeFile = False
        else:
            hHashes[ md5Hash ] = [1,len(data), strippedPath ]

        if writeFile:
            cabNames.append( ( fullPath,
                               str( len( cabNames )+1 )
                           ) )
        else:
            linkNames.append( (hHashes[ md5Hash ][2], strippedPath) )

FCICreate( sys.argv[2] + ".cab", cabNames )
fDir = open( sys.argv[2] + ".cabdir", "wt" )

dirNames = []
for cabEntry in cabNames:
    dirPath = cabEntry[0]
    dirPath = dirname( dirPath )
    dirPath = dirPath[ len(searchPath): ]
    subDirs = dirPath.split( "\\" )
    if 0 == len( subDirs[ -1 ] ):
        subDirs = subDirs[ :-1]

    curDir = ""
    for subDir in subDirs:
        curDir = join( curDir, subDir );
        dirNames.append( "d:" + curDir )

for linkName in linkNames:
    subDirs = dirname( linkName[1] ).split( "\\" )
    if 0 == len( subDirs[ -1 ] ):
        subDirsTree = subDirsTree[ :-1]

    curDir = ""
    for subDir in subDirs:
        curDir = join( curDir, subDir );
        dirNames.append( "d:" + curDir )


dirNames = list( set( dirNames ) )
dirNames.sort()
for dirName in dirNames:
    fDir.write( asUTF8( dirName ) + "\n" )

for cabEntry in cabNames:
    dirPath = cabEntry[0]
    dirPath = dirPath[ len(searchPath): ]
    fDir.write( "f:" + asUTF8( cabEntry[1] ) + ":" + asUTF8( dirPath ) + "\n" )

for linkName in linkNames:
    fDir.write( "l:" + asUTF8( linkName[0] ) + ":" + asUTF8( linkName[1] ) + "\n" )
