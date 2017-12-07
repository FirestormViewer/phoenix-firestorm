import os
import subprocess
import tarfile

class FSViewerManifest:
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

    def fs_delete_linux_symbols( self ):
        debugDir = os.path.join( self.get_dst_prefix(), "bin", ".debug" )

        if os.path.exists( debugDir ):
            from shutil import rmtree
            rmtree( debugDir )

        debugFile = os.path.join( self.get_dst_prefix(), "bin", "debug.log" )

        if os.path.isfile( debugFile ):
            os.unlink( debugFile )
            
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
        debugIndexName = "firestorm-bin.debug.gdb-index"
        debugDir = os.path.join( self.get_dst_prefix(), "bin", ".debug" )
        debugFile = os.path.join( self.get_dst_prefix(), "bin", ".debug", debugName )
        debugIndexFile = os.path.join( self.get_dst_prefix(), "bin", ".debug", debugIndexName )
        if not os.path.exists( debugDir ):
            os.makedirs( debugDir )

        self.run_command_shell( "objcopy %s %s" % (fileSource, debugFile) )

        self.run_command_shell( "gdb -batch -ex \"save gdb-index %s\" %s" % (debugDir, debugFile ) )
        self.run_command_shell( "objcopy --add-section .gdb_index=%s --set-section-flags .gdb_index=readonly %s %s" % (debugIndexFile, debugFile, debugFile) )

        self.run_command_shell( "cd %s && objcopy --add-gnu-debuglink=%s %s" % (debugDir, debugName, fileBin) )
        
        if( os.path.exists( "%s/firestorm-symbols-linux.tar.bz2" % self.args['configuration'].lower()) ):
            symName = "%s/Phoenix_%s_%s_%s_symbols-linux.tar.bz2" % ( self.args['configuration'].lower(), self.fs_channel_legacy_oneword(),
                                                                      '-'.join( self.args['version'] ), self.args['viewer_flavor'] )
            print( "Saving symbols %s" % symName )
            os.rename("%s/firestorm-symbols-linux.tar.bz2" % self.args['configuration'].lower(), symName )

    def fs_linux_tar_excludes(self):
        return "--exclude core --exclude .debug/* --exclude .debug"

    def fs_save_windows_symbols(self, substitution_strings):
        #AO: Try to package up symbols
        # New Method, for reading cross platform stack traces on a linux/mac host
        if (os.path.exists("%s/firestorm-symbols-windows.tar.bz2" % self.args['configuration'].lower())):
            # Rename to add version numbers
            sName = "%s/Phoenix_%s_%s_%s_symbols-windows.tar.bz2" % (self.args['configuration'].lower(),
                                                                     self.fs_channel_legacy_oneword(),
                                                                     substitution_strings['version_dashes'],
                                                                     self.args['viewer_flavor'])

            if os.path.exists( sName ):
                os.unlink( sName )

            os.rename("%s/firestorm-symbols-windows.tar.bz2" % self.args['configuration'].lower(), sName )
        
        pdbName = "firestorm-bin.pdb"
        try:
            subprocess.check_call( [ "pdbcopy.exe" ,
                                     self.args['configuration'] + "\\firestorm-bin.pdb", 
                                     self.args['configuration'] + "\\firestorm-bin-public.pdb",
                                     "-p"
                                 ], stderr=subprocess.PIPE,stdout=subprocess.PIPE )
            pdbName = "firestorm-bin-public.pdb"
        except:
            print( "Cannot run pdbcopy, packaging private symbols" )

        # Store windows symbols we want to keep for debugging in a tar file, this will be later compressed with xz (lzma)
        # Using tat+xz gives far superior compression than zip (~half the size of the zip archive).
        # Python3 natively supports tar+xz via mode 'w:xz'. But we're stuck with Python2 for now.
        symbolTar = tarfile.TarFile("%s/Phoenix_%s_%s_%s_pdbsymbols-windows.tar" % (self.args['configuration'].lower(),
                                                                                    self.fs_channel_legacy_oneword(),
                                                                                    substitution_strings['version_dashes'],
                                                                                    self.args['viewer_flavor']),
                                                                                    'w')
        symbolTar.add( "%s/Firestorm-bin.exe" % self.args['configuration'].lower(), "firestorm-bin.exe" )
        symbolTar.add( "%s/%s" % (self.args['configuration'].lower(),pdbName), pdbName )
        symbolTar.close()

    def fs_strip_windows_manifest(self, aFile ):
        try:
            from win32api import BeginUpdateResource, UpdateResource, EndUpdateResource

            data = None
            handle = BeginUpdateResource( aFile, 0 )
            UpdateResource( handle, 24, 1, data, 1033 )
            EndUpdateResource( handle, 0 )
        except:
            pass

    def fs_copy_windows_manifest(self):
        from shutil import copyfile
        self.fs_strip_windows_manifest( "%s/slplugin.exe" % self.args['configuration'].lower() )
        self.fs_strip_windows_manifest( "%s/llplugin/llceflib_host.exe" % self.args['configuration'].lower() )
        if self.prefix(src=os.path.join(os.pardir, '..', 'indra', 'tools', 'manifests'), dst=""):
            self.path( "compatibility.manifest", "slplugin.exe.manifest" )
            self.end_prefix()
        if self.prefix(src=os.path.join(os.pardir, '..', 'indra', 'tools', 'manifests'), dst="llplugin"):
            self.path( "compatibility.manifest", "llceflib_host.exe.manifest" )
            self.end_prefix()

    def fs_setuid_chromesandbox( self ):
        filename = os.path.join( self.get_dst_prefix(), "bin", "chrome-sandbox" )
        self.run_command_shell( "chmod 755 %s" % ( filename) ) # Strip sticky bit that might be set (in case the following two commands fail)
        self.run_command_shell( "sudo -n chown root:root %s || exit 0" % ( filename) )
        self.run_command_shell( "sudo -n chmod 4755 %s || exit 0" % ( filename) )

