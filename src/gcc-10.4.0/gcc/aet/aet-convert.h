/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

This file is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the file COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/

#ifndef GCC_AET_CONVERT_H
#define GCC_AET_CONVERT_H

 tree aet_convert_to_integer (tree, tree);
 tree aet_convert_to_integer_maybe_fold (tree, tree, bool);
 tree aet_convert_to_pointer (tree, tree);
 tree aet_convert_to_pointer_maybe_fold (tree, tree, bool);
 tree aet_convert_to_real (tree, tree);
 tree aet_convert_to_real_maybe_fold (tree, tree, bool);
 tree aet_convert_to_fixed (tree, tree);
 tree aet_convert_to_complex (tree, tree);
 tree aet_convert_to_complex_maybe_fold (tree, tree, bool);
 tree aet_convert_to_vector (tree, tree);

 inline tree aet_convert_to_integer_nofold (tree t, tree x)
{ return aet_convert_to_integer_maybe_fold (t, x, false); }
 inline tree aet_convert_to_pointer_nofold (tree t, tree x)
{ return aet_convert_to_pointer_maybe_fold (t, x, false); }
 inline tree aet_convert_to_real_nofold (tree t, tree x)
{ return aet_convert_to_real_maybe_fold (t, x, false); }
 inline tree aet_convert_to_complex_nofold (tree t, tree x)
{ return aet_convert_to_complex_maybe_fold (t, x, false); }

 tree aet_preserve_any_location_wrapper (tree result, tree orig_expr);
 tree aet_convert (tree type, tree expr);

#endif /* GCC_CONVERT_H */
