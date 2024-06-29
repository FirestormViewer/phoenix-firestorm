set PATH=C:\CMake\bin;C:\cygwin64\bin;C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x86\;%PATH%
python -m venv .venv
.venv\Scripts\activate.bat
pip install -r requirements.txt
pip install git+https://github.com/secondlife/autobuild.git#egg=autobuild
set_firestorm_vars.bat
set_fmod_vars.bat
copy autobuild.xml my_autobuild.xml
autobuild installables edit fmodstudio platform=windows64 hash=%FMOD_HASH% hash_algorithm=md5 url=%FMOD_URL%
