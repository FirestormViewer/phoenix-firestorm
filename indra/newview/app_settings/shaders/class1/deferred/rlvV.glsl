/**
 *
 * Copyright (c) 2018, Kitty Barnett
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to
 * abide by those obligations.
 *
 */

in vec3 position;

out vec2 vary_fragcoord;

void main()
{
    //transform vertex
    vec4 pos = vec4(position.xyz, 1.0);
    gl_Position = pos;


    vary_fragcoord = (pos.xy*0.5+0.5);
}
