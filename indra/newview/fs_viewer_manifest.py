import os
import subprocess
import tarfile

class FSViewerManifest:
    def fs_installer_basename(self):
        if self.fs_is_avx2():
            opt_string = "AVX2"
        else:
            opt_string = "LEGACY"
        substitution_strings = {
            'version' : '.'.join(self.args['version']),
            'version_short' : '.'.join(self.args['version'][:-1]),
            'version_dashes' : '-'.join(self.args['version']),
            'app_name':self.app_name(),
            'optimized': opt_string,
            'app_name_oneword':self.app_name_oneword()
            }

        return "Phoenix-%(app_name)s_%(optimized)s-%(version_dashes)s" % substitution_strings

    def fs_is_opensim(self):
        return self.args['viewer_flavor'] == 'oss' #Havok would be hvk
    def fs_is_avx2(self):
        return self.args['avx2'] == 'ON' 

    def fs_splice_grid_substitution_strings( self, subst_strings ):
        ret = subst_strings

        if 'grid' in self.args and self.args['grid'] != None:
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
        return self.channel().replace("AyaneStorm", "").strip()

    def fs_sign_win_binaries(self):
        signtool_path = os.getenv('SIGNTOOL_PATH')
        codesigning_dlib_path = os.getenv('CODESIGNING_DLIB_PATH')
        metadata_file = os.getenv("CODESIGNING_METADATA_PATH")
        # at some point we might want to sign other DLLs as well.
        executable_paths = [
            # self.args['configuration'] + "\\firestorm-bin.exe", # no need to sign this we are not packaging it.
            self.args['configuration'] + "\\slplugin.exe",
            self.args['configuration'] + "\\SLVoice.exe",
            self.args['configuration'] + "\\llwebrtc.dll",
            self.args['configuration'] + "\\llplugin\\dullahan_host.exe",
            self.args['configuration'] + "\\" + self.final_exe()
        ]

        if not signtool_path or not codesigning_dlib_path:
            print("Signing configuration is missing. Skipping signing process.")
            return

        print("Signing executables.")
        for exe_path in executable_paths:
            try:
                subprocess.check_call([
                    signtool_path, "sign", "/v", "/debug", "/fd", "SHA256",
                    "/tr", "http://timestamp.acs.microsoft.com", "/td", "SHA256",
                    "/dlib", codesigning_dlib_path, "/dmdf", metadata_file, exe_path
                ], stderr=subprocess.PIPE, stdout=subprocess.PIPE)
                print(f"Signed {exe_path}")
            except Exception as e:
                print(f"Couldn't sign binary: {exe_path}. Error: {e}")

    def fs_sign_win_installer(self, substitution_strings):
        signtool_path = os.getenv('SIGNTOOL_PATH')
        codesigning_dlib_path = os.getenv('CODESIGNING_DLIB_PATH')
        metadata_file = os.getenv("CODESIGNING_METADATA_PATH")
        installer_path = self.args['configuration'] + "\\" + substitution_strings['installer_file']

        if not signtool_path or not codesigning_dlib_path:
            print("Signing configuration is missing. Skipping signing process.")
            return
        print("Signing installer.")

        try:
            subprocess.check_call([
                signtool_path, "sign", "/v", "/debug", "/fd", "SHA256",
                "/tr", "http://timestamp.acs.microsoft.com", "/td", "SHA256",
                "/dlib", codesigning_dlib_path, "/dmdf", metadata_file, installer_path
            ], stderr=subprocess.PIPE, stdout=subprocess.PIPE)
        except Exception as e:
            print(f"Couldn't sign windows installer: {installer_path}. Error: {e}")

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
        print("Packaging symbols")

        self.fs_save_symbols("linux")

    def fs_linux_tar_excludes(self):
        installer_name_components = ['Phoenix',self.app_name(),self.args.get('arch'),'.'.join(self.args['version'])]
        installer_name = "_".join(installer_name_components)
        return "--exclude=%s/bin/.debug" % installer_name

    def fs_save_windows_symbols(self):
        self.fs_save_symbols("windows")

        pdbName = "firestorm-bin.pdb"
        try:
            subprocess.check_call( [ "pdbcopy.exe" ,
                                     self.args['configuration'] + "\\firestorm-bin.pdb", 
                                     self.args['configuration'] + "\\firestorm-bin-public.pdb",
                                     "-p"
                                 ], stderr=subprocess.PIPE,stdout=subprocess.PIPE )
            pdbName = "firestorm-bin-public.pdb"
        except:
            print("Cannot run pdbcopy, packaging private symbols")

        tarName = "%s/Phoenix_%s_%s_%s_pdbsymbols-windows-%d.tar.xz" % (self.args['configuration'].lower(),
                                                                        self.fs_channel_legacy_oneword(),
                                                                        '-'.join(self.args['version']),
                                                                        self.args['viewer_flavor'],
                                                                        self.address_size)                                      
        # Store windows symbols we want to keep for debugging in a tar file.
        symbolTar = tarfile.open( name=tarName, mode="w:xz")
        symbolTar.add( "%s/Firestorm-bin.exe" % self.args['configuration'].lower(), "firestorm-bin.exe" )
        symbolTar.add( "%s/build_data.json" % self.args['configuration'].lower(), "build_data.json" )
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
        self.fs_strip_windows_manifest( "%s/llplugin/dullahan_host.exe" % self.args['configuration'].lower() )
        if self.prefix(src=os.path.join(self.args['build'], os.pardir, os.pardir, 'indra', 'tools', 'manifests')):
            self.path( "compatibility.manifest", "slplugin.exe.manifest" )
            self.end_prefix()
        if self.prefix(src=os.path.join(self.args['build'], os.pardir, os.pardir, 'indra', 'tools', 'manifests'), dst="llplugin"):
            self.path( "compatibility.manifest", "dullahan_host.exe.manifest" )
            self.end_prefix()

    def fs_save_osx_symbols( self ):
        self.fs_save_symbols("darwin")

    def fs_save_symbols(self, osname):
        if (os.path.exists("%s/firestorm-symbols-%s-%d.tar.bz2" % (self.args['configuration'].lower(),
                                                                       osname,
                                                                       self.address_size))):
            # Rename to add version numbers
            sName = "%s/Phoenix_%s_%s_%s_symbols-%s-%d.tar.bz2" % (self.args['configuration'].lower(),
                                                                       self.fs_channel_legacy_oneword(),
                                                                       '-'.join( self.args['version'] ),
                                                                       self.args['viewer_flavor'],
                                                                       osname,
                                                                       self.address_size)

            if os.path.exists( sName ):
                os.unlink( sName )

            os.rename("%s/firestorm-symbols-%s-%d.tar.bz2" % (self.args['configuration'].lower(), osname, self.address_size), sName)

    def fs_generate_breakpad_symbols_for_file( self, aFile ):
        from os import makedirs, remove
        from os.path import join, isfile
        import subprocess
        from shutil import move

        dumpSym = join( self.args["build"], "..", "packages", "bin", "dump_syms" )
        if not isfile( dumpSym ):
            return

        symbolFile = aFile +".sym"

        with open( symbolFile, "w") as outfile:
            subprocess.call( [dumpSym, aFile ], stdout=outfile )

        firstline = open( symbolFile ).readline().strip()

        if firstline != "":
            module, os, bitness, hash, filename = firstline.split(" ")
            symbolDir = join( "symbols", filename, hash )
            try:
                makedirs( symbolDir )
            except:
                pass
            move( symbolFile, symbolDir )

        if isfile( symbolFile ):
            remove( symbolFile )

    def fs_save_breakpad_symbols(self, osname):
        from glob import glob
        import sys
        from os.path import isdir, join
        from shutil import rmtree
        import tarfile

        components = ['Phoenix',self.app_name(),self.args.get('arch'),'.'.join(self.args['version'])]
        symbolsName = "_".join(components)
        symbolsName = symbolsName + "_" + self.args["viewer_flavor"] + "-" + osname + "-" + str(self.address_size) + ".tar.bz2"

        if isdir( "symbols" ):
            rmtree( "symbols" )

        files =  glob( "%s/bin/*" % self.args['dest'] )
        for f in files:
            self.fs_generate_breakpad_symbols_for_file( f )

        files =  glob( "%s/lib/*.so" % self.args['dest'] )
        for f in files:
            if f.find( "libcef.so" ) == -1:
                self.fs_generate_breakpad_symbols_for_file( f )

        if isdir( "symbols" ):
            for a in self.args:
                print("%s: %s" % (a, self.args[a]))

            fTar = tarfile.open( symbolsName, "w:bz2")
            fTar.add("symbols", arcname=".")
            fTar.add( join( self.args["dest"], "build_data.json" ), arcname="build_data.json" )
