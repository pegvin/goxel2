/* Goxel 3D voxels editor
 *
 * copyright (c) 2019-2022 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel.h"

int gui_about_popup(void *data)
{
    gui_text("Goxel2 " GOXEL_VERSION_STR);
    gui_text("a cross-platform 3D voxel art editor extendable via Lua.");
    return gui_button("OK", 0, 0);
}

