// Since we are building an external plugin dll, we need to import from ClassiCube lib instead of exporting these
#define CC_API __declspec(dllimport)
#define CC_VAR __declspec(dllimport)

// NOTE !! Make sure you use the 'Release' configuration when building the dlls to give to end-users.
// Check that the dlls are 25kb instead of debug dll 50-70 kb. (debug dlls depend on MSVCRT debug libs, which end users won't have)

// The proper way would be to add 'additional include directories' and 'additional libs' in Visual Studio Project properties
// Or, you can just be lazy and change these paths for your own system. 
// You must compile ClassiCube in both x86 and x64 configurations to generate the .lib file.
#include "H:/PortableApps/GitPortable/App/Git/ClassicalSharp/src/GameStructs.h"
#include "H:/PortableApps/GitPortable/App/Git/ClassicalSharp/src/Block.h"
#include "H:/PortableApps/GitPortable/App/Git/ClassicalSharp/src/ExtMath.h"
#include "H:/PortableApps/GitPortable/App/Git/ClassicalSharp/src/Game.h"
#include "H:/PortableApps/GitPortable/App/Git/ClassicalSharp/src/Chat.h"
#include "H:/PortableApps/GitPortable/App/Git/ClassicalSharp/src/Stream.h"
#include "H:/PortableApps/GitPortable/App/Git/ClassicalSharp/src/TexturePack.h"
#include "H:/PortableApps/GitPortable/App/Git/ClassicalSharp/src/World.h"

#ifdef _WIN64
#pragma comment(lib, "H:/PortableApps/GitPortable/App/Git/ClassicalSharp/src/x64/Debug/ClassiCube.lib")
#else
#pragma comment(lib, "H:/PortableApps/GitPortable/App/Git/ClassicalSharp/src/x86/Debug/ClassiCube.lib")
#endif


/*########################################################################################################################*
*--------------------------------------------------------Obj exporter-----------------------------------------------------*
*#########################################################################################################################*/
static bool all, mirror;
static struct Stream stream;
static bool include[BLOCK_COUNT]; // which blocks to actually dump vertices of
static int texI[BLOCK_COUNT]; // index mapping for textures 
// buffer data written
static uint8_t buffer[17000];
static int bufferLen;
#define Buffer_Str(str, len) str.buffer = &buffer[bufferLen]; str.length = 0; str.capacity = len;

static void Obj_Init(void) {
	const static String invalid = String_FromConst("Invalid");

	// exports blocks that are not gas draw (air) and are not named "Invalid"
	for (int b = 0; b < BLOCK_COUNT; b++) {
		String name = Block_UNSAFE_GetName(b);
		include[b] = Blocks.Draw[b] != DRAW_GAS && !String_Equals(&name, &invalid);
	}
	bufferLen = 0;
}

static void FlushData(int len) {
	bufferLen += len;
	if (bufferLen < 16384) return;
	uint32_t modified;

	stream.Write(&stream, buffer, bufferLen, &modified);
	bufferLen = 0;
}

static void WriteConst(const char* src) {
	String tmp; Buffer_Str(tmp, 128);
	String_AppendConst(&tmp, src);
	FlushData(tmp.length);
}

#define InitVars()\
	oneX = 1;          maxX = World.MaxX; width  = World.Width;\
	oneZ = width;      maxZ = World.MaxZ; length = World.Length;\
	oneY = World.OneY; maxY = World.MaxY; height = World.Height;\
	blocks = World.Blocks; blocks2 = World.Blocks2;\
	mask = blocks == blocks2 ? 255 : 767;

static void DumpNormals(void) {
	WriteConst("#normals\n");
	WriteConst("vn -1.0 0.0 0.0\n");
	WriteConst("vn 1.0 0.0 0.0\n");
	WriteConst("vn 0.0 0.0 -1.0\n");
	WriteConst("vn 0.0 0.0 1.0\n");
	WriteConst("vn 0.0 -1.0 0.0\n");
	WriteConst("vn 0.0 1.0 0.0\n");
	WriteConst("#sprite normals\n");
	WriteConst("vn -0.70710678 0 0.70710678\n");
	WriteConst("vn 0.70710678 0 -0.70710678\n");
	WriteConst("vn 0.70710678 0 0.70710678\n");
	WriteConst("vn -0.70710678 0 -0.70710678\n");
}


static void Unpack(int texLoc, int* x, int* y) {
	*x = (texLoc % ATLAS2D_TILES_PER_ROW);
	*y = (Atlas2D.RowsCount - 1) - (texLoc / ATLAS2D_TILES_PER_ROW);
}

static void WriteTex(float u, float v) {
	String tmp; Buffer_Str(tmp, 128);
	String_Format2(&tmp, "vt %f8 %f8\n", &u, &v);
	FlushData(tmp.length);
}

static void WriteTexName(int b) {
	String name = Block_UNSAFE_GetName(b);
	String tmp; Buffer_Str(tmp, 128);
	String_Format1(&tmp, "#%s\n", &name);
	FlushData(tmp.length);
}

static void DumpTextures() {
	WriteConst("#textures\n");
	int i = 1;
	int x, y;
	float u = 1.0f / 16, v = 1.0f / Atlas2D.RowsCount;

	for (int b = 0; b < BLOCK_COUNT; b++) {
		if (!include[b]) continue;
		WriteTexName(b);

		Vector3 min = Blocks.MinBB[b], max = Blocks.MaxBB[b];	
		if (Blocks.Draw[b] == DRAW_SPRITE) {
			min = Vector3_Zero();
			max = Vector3_One();
		}
		texI[b] = i;

		Unpack(Block_Tex(b, FACE_XMIN), &x, &y);
		WriteTex((x + min.Z) * u, (y + min.Y) * v);
		WriteTex((x + min.Z) * u, (y + max.Y) * v);
		WriteTex((x + max.Z) * u, (y + max.Y) * v);
		WriteTex((x + max.Z) * u, (y + min.Y) * v);

		Unpack(Block_Tex(b, FACE_XMAX), &x, &y);
		WriteTex((x + max.Z) * u, (y + min.Y) * v);
		WriteTex((x + max.Z) * u, (y + max.Y) * v);
		WriteTex((x + min.Z) * u, (y + max.Y) * v);
		WriteTex((x + min.Z) * u, (y + min.Y) * v);

		Unpack(Block_Tex(b, FACE_ZMIN), &x, &y);
		WriteTex((x + max.X) * u, (y + min.Y) * v);
		WriteTex((x + max.X) * u, (y + max.Y) * v);
		WriteTex((x + min.X) * u, (y + max.Y) * v);
		WriteTex((x + min.X) * u, (y + min.Y) * v);

		Unpack(Block_Tex(b, FACE_ZMAX), &x, &y);
		WriteTex((x + min.X) * u, (y + min.Y) * v);
		WriteTex((x + min.X) * u, (y + max.Y) * v);
		WriteTex((x + max.X) * u, (y + max.Y) * v);
		WriteTex((x + max.X) * u, (y + min.Y) * v);

		Unpack(Block_Tex(b, FACE_YMIN), &x, &y);
		WriteTex((x + min.X) * u, (y + max.Z) * v);
		WriteTex((x + min.X) * u, (y + min.Z) * v);
		WriteTex((x + max.X) * u, (y + min.Z) * v);
		WriteTex((x + max.X) * u, (y + max.Z) * v);

		Unpack(Block_Tex(b, FACE_YMAX), &x, &y);
		WriteTex((x + min.X) * u, (y + max.Z) * v);
		WriteTex((x + min.X) * u, (y + min.Z) * v);
		WriteTex((x + max.X) * u, (y + min.Z) * v);
		WriteTex((x + max.X) * u, (y + max.Z) * v);
		i += 4 * 6;
	}
}

static bool IsFaceHidden(int block, int other, int side) {
	return (Blocks.Hidden[(block * BLOCK_COUNT) + other] & (1 << side)) != 0;
}

static void WriteVertex(float x, float y, float z) {
	String tmp; Buffer_Str(tmp, 256);
	String_Format3(&tmp, "v %f8 %f8 %f8\n", &x, &y, &z);
	FlushData(tmp.length);
}

static RNGState spriteRng;
static void DumpVertices() {
	WriteConst("#vertices\n");
	int i = -1, mask;
	Vector3 min, max;

	int oneX, maxX, width;
	int oneY, maxY, height;
	int oneZ, maxZ, length;
	uint8_t *blocks, *blocks2;
	InitVars();

	for (int y = 0; y < height; y++) {
		for (int z = 0; z < length; z++) {
			for (int x = 0; x < width; x++) {
				++i; int b = (blocks[i] | (blocks2[i] << 8)) & mask;
				if (!include[b]) continue;
				min.X = x; min.Y = y; min.Z = z;
				max.X = x; max.Y = y; max.Z = z;

				if (Blocks.Draw[b] == DRAW_SPRITE) {
					min.X += 2.50f / 16; min.Z += 2.50f / 16;
					max.X += 13.5f / 16; max.Z += 13.5f / 16; max.Y += 1.0f;

					int offsetType = Blocks.SpriteOffset[b];
					if (offsetType >= 6 && offsetType <= 7) {
						Random_Seed(&spriteRng, (x + 1217 * z) & 0x7fffffff);
						float valX = Random_Range(&spriteRng, -3, 3 + 1) / 16.0f;
						float valY = Random_Range(&spriteRng,  0, 3 + 1) / 16.0f;
						float valZ = Random_Range(&spriteRng, -3, 3 + 1) / 16.0f;

						const float stretch = 1.7f / 16.0f;
						min.X += valX - stretch; max.X += valX + stretch;
						min.Z += valZ - stretch; max.Z += valZ + stretch;
						if (offsetType == 7) { min.Y -= valY; max.Y -= valY; }
					}

					// Draw Z axis
					WriteVertex(min.X, min.Y, min.Z);
					WriteVertex(min.X, max.Y, min.Z);
					WriteVertex(max.X, max.Y, max.Z);
					WriteVertex(max.X, min.Y, max.Z);

					// Draw Z axis mirrored
					if (mirror) {
						WriteVertex(max.X, min.Y, max.Z);
						WriteVertex(max.X, max.Y, max.Z);
						WriteVertex(min.X, max.Y, min.Z);
						WriteVertex(min.X, min.Y, min.Z);
					}

					// Draw X axis
					WriteVertex(min.X, min.Y, max.Z);
					WriteVertex(min.X, max.Y, max.Z);
					WriteVertex(max.X, max.Y, min.Z);
					WriteVertex(max.X, min.Y, min.Z);

					// Draw X axis mirrored
					if (mirror) {
						WriteVertex(max.X, min.Y, min.Z);
						WriteVertex(max.X, max.Y, min.Z);
						WriteVertex(min.X, max.Y, max.Z);
						WriteVertex(min.X, min.Y, max.Z);
					}
					continue;
				}

				Vector3_AddBy(&min, &Blocks.RenderMinBB[b]);
				Vector3_AddBy(&max, &Blocks.RenderMaxBB[b]);

				// minx
				if (x == 0 || all || !IsFaceHidden(b, (blocks[i - oneX] | (blocks2[i - oneX] << 8)) & mask, FACE_XMIN)) {
					WriteVertex(min.X, min.Y, min.Z);
					WriteVertex(min.X, max.Y, min.Z);
					WriteVertex(min.X, max.Y, max.Z);
					WriteVertex(min.X, min.Y, max.Z);
				}

				// maxx
				if (x == maxX || all || !IsFaceHidden(b, (blocks[i + oneX] | (blocks2[i + oneX] << 8)) & mask, FACE_XMAX)) {
					WriteVertex(max.X, min.Y, min.Z);
					WriteVertex(max.X, max.Y, min.Z);
					WriteVertex(max.X, max.Y, max.Z);
					WriteVertex(max.X, min.Y, max.Z);
				}

				// minz
				if (z == 0 || all || !IsFaceHidden(b, (blocks[i - oneZ] | (blocks2[i - oneZ] << 8)) & mask, FACE_ZMIN)) {
					WriteVertex(min.X, min.Y, min.Z);
					WriteVertex(min.X, max.Y, min.Z);
					WriteVertex(max.X, max.Y, min.Z);
					WriteVertex(max.X, min.Y, min.Z);
				}

				// maxz
				if (z == maxZ || all || !IsFaceHidden(b, (blocks[i + oneZ] | (blocks2[i + oneZ] << 8)) & mask, FACE_ZMAX)) {
					WriteVertex(min.X, min.Y, max.Z);
					WriteVertex(min.X, max.Y, max.Z);
					WriteVertex(max.X, max.Y, max.Z);
					WriteVertex(max.X, min.Y, max.Z);
				}

				// miny
				if (y == 0 || all || !IsFaceHidden(b, (blocks[i - oneY] | (blocks2[i - oneY] << 8)) & mask, FACE_YMIN)) {
					WriteVertex(min.X, min.Y, min.Z);
					WriteVertex(min.X, min.Y, max.Z);
					WriteVertex(max.X, min.Y, max.Z);
					WriteVertex(max.X, min.Y, min.Z);
				}

				// maxy
				if (y == maxY || all || !IsFaceHidden(b, (blocks[i + oneY] | (blocks2[i + oneY] << 8)) & mask, FACE_YMAX)) {
					WriteVertex(min.X, max.Y, min.Z);
					WriteVertex(min.X, max.Y, max.Z);
					WriteVertex(max.X, max.Y, max.Z);
					WriteVertex(max.X, max.Y, min.Z);
				}
			}
		}
	}
}

static void WriteFace(int v1,int t1,int n1, int v2,int t2,int n2, int v3,int t3,int n3, int v4,int t4,int n4) {
	String tmp; Buffer_Str(tmp, 256);
	String_Format3(&tmp,"f %i/%i/%i ", &v1, &t1, &n1);
	String_Format3(&tmp,  "%i/%i/%i ", &v2, &t2, &n2);
	String_Format3(&tmp,  "%i/%i/%i ", &v3, &t3, &n3);
	String_Format3(&tmp,  "%i/%i/%i\n",&v4, &t4, &n4);
	FlushData(tmp.length);
}

static void DumpFaces() {
	WriteConst("#faces\n");
	int i = -1, j = 1, mask;

	int oneX, maxX, width;
	int oneY, maxY, height;
	int oneZ, maxZ, length;
	uint8_t *blocks, *blocks2;
	InitVars();

	for (int y = 0; y < height; y++) {
		for (int z = 0; z < length; z++) {
			for (int x = 0; x < width; x++) {
				++i; int b = (blocks[i] | (blocks2[i] << 8)) & mask;
				if (!include[b]) continue;
				int k = texI[b], n = 1;

				if (Blocks.Draw[b] == DRAW_SPRITE) {
					n += 6;
					WriteFace(j+3,k+3,n, j+2,k+2,n, j+1,k+1,n, j+0,k+0,n); j += 4; n++;
					if (mirror) { WriteFace(j+3,k+3,n, j+2,k+2,n, j+1,k+1,n, j+0,k+0,n); j += 4; n++; }

					WriteFace(j+3,k+3,n, j+2,k+2,n, j+1,k+1,n, j+0,k+0,n); j += 4; n++;
					if (mirror) { WriteFace(j+3,k+3,n, j+2,k+2,n, j+1,k+1,n, j+0,k+0,n); j += 4; n++; }
					continue;
				}

				// minx
				if (x == 0 || all || !IsFaceHidden(b, (blocks[i - oneX] | (blocks2[i - oneX] << 8)) & mask, FACE_XMIN)) {
					WriteFace(j+3,k+3,n, j+2,k+2,n, j+1,k+1,n, j+0,k+0,n); j += 4;
				} k += 4; n++;

				// maxx
				if (x == maxX || all || !IsFaceHidden(b, (blocks[i + oneX] | (blocks2[i + oneX] << 8)) & mask, FACE_XMAX)) {
					WriteFace(j+0,k+0,n, j+1,k+1,n, j+2,k+2,n, j+3,k+3,n); j += 4;
				} k += 4; n++;

				// minz
				if (z == 0 || all || !IsFaceHidden(b, (blocks[i - oneZ] | (blocks2[i - oneZ] << 8)) & mask, FACE_ZMIN)) {
					WriteFace(j+0,k+0,n, j+1,k+1,n, j+2,k+2,n, j+3,k+3,n); j += 4;
				} k += 4; n++;

				// maxz
				if (z == maxZ || all || !IsFaceHidden(b, (blocks[i + oneZ] | (blocks2[i + oneZ] << 8)) & mask, FACE_ZMAX)) {
					WriteFace(j+3,k+3,n, j+2,k+2,n, j+1,k+1,n, j+0,k+0,n); j += 4;
				} k += 4; n++;

				// miny
				if (y == 0 || all || !IsFaceHidden(b, (blocks[i - oneY] | (blocks2[i - oneY] << 8)) & mask, FACE_YMIN)) {
					WriteFace(j+3,k+3,n, j+2,k+2,n, j+1,k+1,n, j+0,k+0,n); j += 4;
				} k += 4; n++;

				// maxy
				if (y == maxY || all || !IsFaceHidden(b, (blocks[i + oneY] | (blocks2[i + oneY] << 8)) & mask, FACE_YMAX)) {
					WriteFace(j+0,k+0,n, j+1,k+1,n, j+2,k+2,n, j+3,k+3,n); j += 4;
				} k += 4; n++;
			}
		}
	}
}

static void ExportObj(void) {
	Obj_Init();
	DumpNormals();
	DumpTextures();
	DumpVertices();
	DumpFaces();

	if (!bufferLen) return;
	uint32_t modified;

	stream.Write(&stream, buffer, bufferLen, &modified);
	bufferLen = 0;
}

/*########################################################################################################################*
*---------------------------------------------------Plugin implementation-------------------------------------------------*
*#########################################################################################################################*/
#define SendChat(msg) const static String str = String_FromConst(msg); Chat_Add(&str);

static void ObjExporterCommand_Execute(const String* args, int argsCount) {
	if (!argsCount) { SendChat("&cFilename required."); return; }

	char strBuffer[FILENAME_SIZE];
	String str = String_FromArray(strBuffer);
	String_Format1(&str, "maps/%s.obj", &args[0]);

	ReturnCode res = Stream_CreateFile(&stream, &str);
	if (res) {
		str.length = 0;
		String_Format2(&str, "error %h creating maps/%s.obj", &res, &args[0]);
		Chat_Add(&str);
		return;
	}

	all = argsCount > 1 && String_CaselessEqualsConst(&args[1], "ALL");
	if (all) { SendChat("&cExporting all faces - slow!"); }

	mirror = argsCount <= 2 || !String_CaselessEqualsConst(&args[2], "NO");
	if (!mirror) { SendChat("&cNot mirroring sprites!"); }

	ExportObj();
	stream.Close(&stream);

	str.length = 0;
	String_Format1(&str, "&eExported map to maps/%s.obj", &args[0]);
	Chat_Add(&str);
}

static struct ChatCommand ObjExporterCommand = {
	"ObjExport", ObjExporterCommand_Execute, false,
	{
		"&a/client objexport [filename] ['all' for all faces] ['no' for not mirror]",
		"&eExports the current map to the OBJ file format, as [filename].obj.",
		"&e  (excluding faces of blocks named 'Invalid')",
		"&eThis can then be imported into 3D modelling software such as Blender.",
	}
};

static void ObjExporter_Init(void) {
	Commands_Register(&ObjExporterCommand);
}


/*########################################################################################################################*
*----------------------------------------------------Plugin boilerplate---------------------------------------------------*
*#########################################################################################################################*/
__declspec(dllexport) int Plugin_ApiVersion = 1;
__declspec(dllexport) struct IGameComponent Plugin_Component = {
	ObjExporter_Init /* Init */
};