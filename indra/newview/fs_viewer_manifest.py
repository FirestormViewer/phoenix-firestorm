import os
import subprocess

class FSViewerManifest:
    def fs_is_64bit_build( self ):
        return self.args.has_key( 'm64' )

    def fs_flavor( self ):
        return self.args['viewer_flavor']  # [oss or hvk]
    
    def fs_splice_grid_substitution_strings( self, subst_strings ):
        ret = subst_strings

        if self.args.has_key( 'grid' ) and self.args['grid'] != None:
          ret[ 'grid' ] = self.args['grid']
          ret[ 'grid_caps' ] = self.args['grid'].upper()
        else:
          ret[ 'grid' ] = ""
          ret[ 'grid_caps' ] = ""

        return ret

    def fs_get_substitution_strings( self ):
        substitution_strings = {
            'version' : '.'.join(self.args['version']),
            'version_short' : '.'.join(self.args['version'][:-1]),
            'version_dashes' : '-'.join(self.args['version']),
            'channel':self.channel(),
            'channel_oneword':self.fs_channel_oneword(),
            'channel_unique':self.fs_channel_unique(),
            'subchannel_underscores':'_'.join(self.fs_channel_unique().split()),
            'app_name' : self.app_name()
        }

        return self.fs_splice_grid_substitution_strings( substitution_strings )

    def fs_channel_legacy_oneword(self):
        return "".join(self.channel().split())
    
    def fs_channel_oneword(self):
        return "".join(self.fs_channel_unique().split())

    def fs_channel_unique(self):
        return self.channel().replace("Firestorm", "").strip()

    def fs_sign_win_binaries( self ):
        try:
            subprocess.check_call(["signtool.exe","sign","/n","Phoenix","/d","Firestorm","/du","http://www.phoenixviewer.com","/t","http://timestamp.verisign.com/scripts/timstamp.dll",self.args['configuration']+"\\firestorm-bin.exe"],
                                  stderr=subprocess.PIPE,stdout=subprocess.PIPE)
            subprocess.check_call(["signtool.exe","sign","/n","Phoenix","/d","Firestorm","/du","http://www.phoenixviewer.com","/t","http://timestamp.verisign.com/scripts/timstamp.dll",self.args['configuration']+"\\slplugin.exe"],
                                  stderr=subprocess.PIPE,stdout=subprocess.PIPE)
            subprocess.check_call(["signtool.exe","sign","/n","Phoenix","/d","Firestorm","/du","http://www.phoenixviewer.com","/t","http://timestamp.verisign.com/scripts/timstamp.dll",self.args['configuration']+"\\SLVoice.exe"],
                                  stderr=subprocess.PIPE,stdout=subprocess.PIPE)
            subprocess.check_call(["signtool.exe","sign","/n","Phoenix","/d","Firestorm","/du","http://www.phoenixviewer.com","/t","http://timestamp.verisign.com/scripts/timstamp.dll",self.args['configuration']+"\\"+self.final_exe()],
                                  stderr=subprocess.PIPE,stdout=subprocess.PIPE)
        except Exception, e:
            print "Couldn't sign final binary. Tried to sign %s" % self.args['configuration']+"\\"+self.final_exe()

    def fs_sign_win_installer( self, substitution_strings ):
        try:
            subprocess.check_call(["signtool.exe","sign","/n","Phoenix","/d","Firestorm","/du","http://www.phoenixviewer.com",self.args['configuration']+"\\"+substitution_strings['installer_file']],stderr=subprocess.PIPE,stdout=subprocess.PIPE)
        except Exception, e:
            print "Working directory: %s" % os.getcwd()
            print "Couldn't sign windows installer. Tried to sign %s" % self.args['configuration']+"\\"+substitution_strings['installer_file']

    def fs_save_linux_symbols( self ):
        #AO: Try to package up symbols
        # New Method, for reading cross platform stack traces on a linux/mac host
        print( "Packaging symbols" )

        """
         Copy symbols into a .debug subdir, then add a debug link into the stripped exe.
         This allows gdb to automatically pick up symbols, even when they are not embedded.
         Maybe at some point it is worth to extract the build-id (readelf -n) and store the
         symbols in a .symbol server in form of xy/za*.debug where xyza* is the build-id.
        """ 
        
        fileBin = os.path.join( self.get_dst_prefix(), "bin", "do-not-directly-run-firestorm-bin" )
        fileSource = os.path.join( self.get_dst_prefix(), "..", "firestorm-bin" )

        debugName = "firestorm-bin.debug"
        debugDir = os.path.join( self.get_dst_prefix(), "bin", ".debug" )
        debugFile = os.path.join( self.get_dst_prefix(), "bin", ".debug", debugName )
        if not os.path.exists( debugDir ):
            os.makedirs( debugDir )

        self.run_command( "objcopy %s %s" % (fileSource, debugFile) )
        self.run_command( "cd %s && objcopy --add-gnu-debuglink=%s %s" % (debugDir, debugName, fileBin) )
        
        if( os.path.exists( "%s/firestorm-symbols-linux.tar.bz2" % self.args['configuration'].lower()) ):
            symName = "%s/Phoenix_%s_%s_%s_symbols-linux.tar.bz2" % ( self.args['configuration'].lower(), self.fs_channel_legacy_oneword(),
                                                                      '-'.join( self.args['version'] ), self.args['viewer_flavor'] )
            print( "Saving symbols %s" % symName )
            os.rename("%s/firestorm-symbols-linux.tar.bz2" % self.args['configuration'].lower(), symName )

    def fs_linux_tar_excludes(self):
        return "--exclude core --exclude .debug/* --exclude .debug"
