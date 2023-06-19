MeshImport: Written by John W. Ratcliff mailto:jratcliffscarab@gmail.com

Website: http://www.codesuppository.blogspot.com

This source is released under the MIT license.

This sample program demonstrates how to use the MeshImport library.

The MeshImport library allows a developer to easily import and export
skeletal animated and mesh deformed data.

The MeshImport library is provided a series of DLL components.

The main component 'MeshImport.dll' provides support to serialize
and deserialize skeletal mesh data.

An additional set of DLLs provide importers for a variety of
specific graphics file formats.

They include the following formats:

EZMesh	- A simple XML data description that is a reflective object model definition of the MeshImport data structures.
FBX     - A file format used by 3D Studio Max and Maya
OBJ	    - The classic Wavefront OBJ file format.  Only supports mesh data, not skeletons or animation.
OGRE	- A skeletal mesh format used by the OGRE3D open source game engine.
PSK		- A skeletal mesh format supported by the Unreal Engine from Epic.

The source code to the DLL components is 'open source' and available on GoogleCode at

http://code.google.com/p/meshimport/

The source there may or may not be current.  It is recommanded that you stick to
using the pre-built binaries provided here.

Since the MeshImport library uses dynamically loaded DLLs the only thing you
need to use it is 'MeshImport.h' and 'MeshImport.cpp'

Since these are DLL components, this code only runs on Windows platforms.
You could build the raw-source for other platforms if you wish, and I have
done that myself, even for game consoles.

The entire adata defintion for the skeletal mesh animated data is defined
in the single header file 'MeshImport.h' with no external dependencies.

A simple demonstration program that shows how to both import and export
meshes can be found in main.cpp

It is strongly recommended that you use the EZ-Mesh file format.

It was designed to be compact and easily human readable for debugging purposes.

You might legitimately ask why would I create 'yet another graphics file format'.

Aren't there plenty of them already?  Isn't that what COLLADA is supopsed to do?

The problem is, that I have never found any graphics file format that meets my
minimum basic requirements, which are:

(1) The file format should be self-documenting and extremely human readable.
    Simply take a look at EXPORT.EZM which is only 34 lines long yet specifies
    a skeleton, animation data, and mesh data in a single file.
    The same thing with COLLADA would be mind blowing in complexity.

(2) The source code necessary to read the file format should be small.
	All of the source necessary to import an EZmesh file is less than 1,000
	lines of code; and that includes the entire XML parser!
	In contrast COLLADA uses libraries that are composed of literally
	hundreds of thousands of lines of source to do just the same thing.

(3) The file format should allow a user to easily simply 'fprintf' or 'stream out'
	the contents of arbitrary mesh data to a file on disk.  Ez-mesh lets you do this.

(4) The file format should be able to very simply express skeletons, bones, animations,
    and a wide variety of vertex formats.  Again, EZ-mesh does this and other formats
    do not.

(5) The library should require no external dependencies, not on STL, and not ony any
    vector, matrix, or quaternion libraries.  The header file 'MeshImport.h' provides
    a few simple helper math functions if necessary.  All data is describes as simple
    arrays of floats.

If such a file format already existed that did all of this, I would happily use it.
However, there are none so I invented EZ-mesh for exactly this purpose.

In addition to skeletal deformed mesh data, Ez-mesh can also describe arbitrary user
data, in both binary and ascii, as well as collision primitives such as boxes,
spheres, capsules, and convex hulls.

EZ-Mesh was originally developed as part of the open source project 'ODF' (the
open dynamics framework) when I worked for Ageia technologies.  Today I work as
a senior software engineer at NVIDIA corporation and I am still maintaining the
format today.

Below is a brief description of the files contained in this package:

ClothSim.ezm		: A full mesh deformed skeletal animated character.
export.ezm			: A demonstration exported EZM file that defines each of the elements available for deformed skeletal meshes.
main.cpp			: A tiny sample program to demonstrate how to use the mesh import library.
MeshImport.cpp		: The binding code to load the MeshImport.dll and associated importeres.
MeshImport.h		: The single header file necessary to use the MeshImport library
MeshImport.dll		: The main mesh import DLL which supports serialization of mesh data.
MeshImport.sln	    : A VC2005 solution file to build the sample program.
MeshImportEzm.dll	: The EZ-Mesh importer dll
MeshImportFbx.dll	: The FBX importer dll
MeshImportObj.dll	: The Wavefront OBJ importer DLL
MeshImportOgre.dll	: The Ogre3d importer DLL
MeshImportPSK.dll	: The Unreal PSK file format importer DLL
