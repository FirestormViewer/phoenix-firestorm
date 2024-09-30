# build_config.py

import json

class BuildConfig:
    def __init__(self, config_file='./fsutils/build_config.json'):
        with open(config_file, 'r') as f:
            config_data = json.load(f)

        self.supported_os_dirs = config_data.get('os_download_dirs', [])
        # channel_to_build_type is a map from Beta, Release and Nightly to folder names preview release and nightly
        self.build_type_hosted_folder = config_data.get('build_type_hosted_folder', {})
        self.fs_version_mgr_platform = config_data.get('fs_version_mgr_platform', {})
        self.os_hosted_folder = config_data.get('os_hosted_folder', {})
        self.platforms_printable = config_data.get('platforms_printable', {})
        self.grids_printable = config_data.get('grids_printable', {})
        self.download_root = config_data.get('download_root', '')
        self.viewer_channel_mapping = config_data.get('viewer_channel_mapping', {})
        self.variant_printable = config_data.get('variant_printable', {})
