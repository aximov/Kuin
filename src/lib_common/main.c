// LibCommon.dll
//
// (C)Kuina-chan
//

#include "main.h"

#include "rnd.h"

typedef enum ETypeId
{
	TypeId_Int = 0x00,
	TypeId_Float = 0x01,
	TypeId_Char = 0x02,
	TypeId_Bool = 0x03,
	TypeId_Bit8 = 0x04,
	TypeId_Bit16 = 0x05,
	TypeId_Bit32 = 0x06,
	TypeId_Bit64 = 0x07,
	TypeId_Func = 0x08,
	TypeId_Enum = 0x09,
	TypeId_Array = 0x80,
	TypeId_List = 0x81,
	TypeId_Stack = 0x82,
	TypeId_Queue = 0x83,
	TypeId_Dict = 0x84,
	TypeId_Class = 0x85,
} ETypeId;

S128 RndMask;
static SRndState GlobalRnd;

static Bool IsRef(U8 type);
static int GetSize(U8 type);
static Bool Same(double f1, double f2);
static S64 Add(S64 a, S64 b);
static S64 Mul(S64 a, S64 b);
static void GetDictTypes(const U8* type, const U8** child1, const U8** child2);
static void GetDictTypesRecursion(const U8** type);
static void FreeDict(void* ptr, const U8* child1, const U8* child2);
static Bool IsStr(const U8* type);
static Bool IsSpace(Char c);
static void Copy(void* dst, U8 type, const void* src);
static void* AddDictRecursion(void* node, const void* key, const void* item, int cmp_func(const void* a, const void* b), U8* key_type, U8* item_type, Bool* addition);
static void* CopyDictRecursion(void* node, U8* key_type, U8* item_type);
static void ToBinDictRecursion(void** buf, void* node, U8* key_type, U8* item_type);
static void* FromBinDictRecursion(U8* key_type, U8* item_type, const U8* bin, S64* idx);
static int(*GetCmpFunc(const U8* type))(const void* a, const void* b);
static int CmpInt(const void* a, const void* b);
static int CmpFloat(const void* a, const void* b);
static int CmpChar(const void* a, const void* b);
static int CmpBit8(const void* a, const void* b);
static int CmpBit16(const void* a, const void* b);
static int CmpBit32(const void* a, const void* b);
static int CmpBit64(const void* a, const void* b);
static int CmpStr(const void* a, const void* b);
static int CmpClass(const void* a, const void* b);
static void* CatBin(int num, void** bins);

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
	UNUSED(hinst);
	UNUSED(reason);
	UNUSED(reserved);
	return TRUE;
}

EXPORT S64 rnd(S64 min, S64 max)
{
	return RndGet(&GlobalRnd, min, max);
}

EXPORT double rndFloat(double min, double max)
{
	return RndGetFloat(&GlobalRnd, min, max);
}

EXPORT U64 rndBit64(void)
{
	return RndGetBit64(&GlobalRnd);
}

EXPORT void _init(void* heap, S64* heap_cnt)
{
	Heap = heap;
	HeapCnt = heap_cnt;

	// Initialize the random number system.
	RndMask = _mm_set_epi32(0x7ff7fb2f, 0xff777b7d, 0xef7f3f7d, 0xfdff37ff);
#if defined(_DEBUG)
	// Test the random number system.
	{
		const U64 answers[] =
		{
			0x33ff358570beb516, 0xa09f66b21c23687b, 0x34506b19caf13173, 0x47bbd348fd8e122f,
			0xcb2fb52e99922f80, 0x7b633b3d7230b48d, 0xd7bb5c4f79c6886b, 0x27b2d2079e86b7da,
			0xb801da316661da6a, 0xfb20bf53344a71c4, 0xdd26c89a30d3fefe, 0x4d291ef4ed2381d5,
			0x03a063c847570621, 0x803d64a732ebe145, 0xf7d6f2d0bc4906be, 0xf44552c12646cd84,
			0x0a4df6f031fb46b3, 0x894ebd381ff0c2a8, 0x34d2221d79c8b86e, 0xf68cf4fdd4f5a265,
			0xa6dd0d5bdf172c87, 0xc3bd1fd6a7702d36, 0xb8e8c39722379c82, 0xa8e0c595ed87c7f6,
			0x368d2b6111065b24, 0x57ae24e8fc53eefc, 0xe80e4d6647ace128, 0xa323547aa04421b2,
			0x5cf311d7ea76c8c3, 0xa160420e0f3bf087, 0x31c91cf6323bcaf2, 0x341fd6794ac66886,
		};
		int i;
		RndInit(&GlobalRnd, 917);
		for (i = 0; i < sizeof(answers) / sizeof(answers[0]); i++)
		{
			U64 n = RndGetBit64(&GlobalRnd);
			ASSERT(n == answers[i]);
		}
	}
#endif
	RndInit(&GlobalRnd, (U32)(time(NULL)) ^ (U32)timeGetTime() ^ 0x2971c37b);
}

EXPORT void _fin(void)
{
	// TODO:
}

EXPORT void _err(const U8* excpt)
{
	// TODO:
}

EXPORT void _freeSet(void* ptr, const U8* type)
{
	switch (*type)
	{
		case TypeId_Array:
			type++;
			if (IsRef(*type))
			{
				S64 len = ((S64*)ptr)[1];
				void** ptr2 = (void**)((U8*)ptr + 0x10);
				while (len != 0)
				{
					if (*ptr2 != NULL)
					{
						(*(S64*)*ptr2)--;
						if (*(S64*)*ptr2 == 0)
							_freeSet(*ptr2, type);
					}
					ptr2++;
					len--;
				}
			}
			FreeMem(ptr);
			break;
		case TypeId_List:
			type++;
			{
				void* ptr2 = *(void**)((U8*)ptr + 0x10);
				while (ptr2 != NULL)
				{
					void* ptr3 = ptr2;
					if (IsRef(*type))
					{
						void* ptr4 = *(void**)((U8*)ptr2 + 0x10);
						(*(S64*)ptr4)--;
						if (*(S64*)ptr4 == 0)
							_freeSet(ptr4, type);
					}
					ptr2 = *(void**)((U8*)ptr2 + 0x08);
					FreeMem(ptr3);
				}
			}
			FreeMem(ptr);
			break;
		case TypeId_Stack:
			type++;
			{
				void* ptr2 = *(void**)((U8*)ptr + 0x10);
				while (ptr2 != NULL)
				{
					void* ptr3 = ptr2;
					if (IsRef(*type))
					{
						void* ptr4 = *(void**)((U8*)ptr2 + 0x08);
						(*(S64*)ptr4)--;
						if (*(S64*)ptr4 == 0)
							_freeSet(ptr4, type);
					}
					ptr2 = *(void**)ptr2;
					FreeMem(ptr3);
				}
			}
			FreeMem(ptr);
			break;
		case TypeId_Queue:
			type++;
			{
				void* ptr2 = *(void**)((U8*)ptr + 0x10);
				while (ptr2 != NULL)
				{
					void* ptr3 = ptr2;
					if (IsRef(*type))
					{
						void* ptr4 = *(void**)((U8*)ptr2 + 0x08);
						(*(S64*)ptr4)--;
						if (*(S64*)ptr4 == 0)
							_freeSet(ptr4, type);
					}
					ptr2 = *(void**)ptr2;
					FreeMem(ptr3);
				}
			}
			FreeMem(ptr);
			break;
		case TypeId_Dict:
			{
				U8* child1;
				U8* child2;
				GetDictTypes(type, &child1, &child2);
				FreeDict(*(void**)((U8*)ptr + 0x10), child1, child2);
				FreeMem(ptr);
			}
			break;
		default:
			ASSERT(*type == TypeId_Class);
			// TODO:
			break;
	}
}

EXPORT void* _copy(const void* me_, const U8* type)
{
	ASSERT(me_ != NULL);
	switch (*type)
	{
		case TypeId_Array:
			{
				int size = GetSize(type[1]);
				S64 len = *(S64*)((U8*)me_ + 0x08);
				Bool is_str = IsStr(type);
				U8* result = (U8*)AllocMem(0x10 + (size_t)size * (size_t)(len + (is_str ? 1 : 0)));
				((S64*)result)[0] = DefaultRefCntOpe;
				((S64*)result)[1] = len;
				if (IsRef(type[1]))
				{
					S64 i;
					void** src = (void**)((U8*)me_ + 0x10);
					void** dst = (void**)(result + 0x10);
					for (i = 0; i < len; i++)
					{
						if (*src == NULL)
							*dst = NULL;
						else
							*dst = _copy(*src, type + 1);
						src++;
						dst++;
					}
				}
				else
					memcpy(result + 0x10, (U8*)me_ + 0x10, (size_t)size * (size_t)(len + (is_str ? 1 : 0)));
				return result;
			}
		case TypeId_List:
			{
				int size = GetSize(type[1]);
				Bool is_ref = IsRef(type[1]);
				U8* result = (U8*)AllocMem(0x28);
				((S64*)result)[0] = DefaultRefCntOpe;
				((S64*)result)[1] = *(S64*)((U8*)me_ + 0x08);
				((S64*)result)[2] = 0;
				((S64*)result)[3] = 0;
				((S64*)result)[4] = 0;
				{
					void* src = *(void**)((U8*)me_ + 0x10);
					while (src != NULL)
					{
						U8* node = (U8*)AllocMem(0x10 + (size_t)size);
						if (is_ref)
						{
							if (*(void**)((U8*)src + 0x10) == NULL)
								*(void**)(node + 0x10) = NULL;
							else
								*(void**)(node + 0x10) = _copy(*(void**)((U8*)src + 0x10), type + 1);
						}
						else
							memcpy(node + 0x10, (U8*)src + 0x10, (size_t)size);
						*(void**)(node + 0x08) = NULL;
						if (*(void**)(result + 0x10) == NULL)
						{
							*(void**)node = NULL;
							*(void**)(result + 0x10) = node;
							*(void**)(result + 0x18) = node;
						}
						else
						{
							*(void**)node = *(void**)(result + 0x18);
							*(void**)((U8*)*(void**)(result + 0x18) + 0x08) = node;
							*(void**)(result + 0x18) = node;
						}
						src = *(void**)((U8*)src + 0x08);
					}
				}
				return result;
			}
		case TypeId_Stack:
			{
				int size = GetSize(type[1]);
				Bool is_ref = IsRef(type[1]);
				void* top = NULL;
				void* bottom = NULL;
				U8* result = (U8*)AllocMem(0x18);
				((S64*)result)[0] = DefaultRefCntOpe;
				((S64*)result)[1] = *(S64*)((U8*)me_ + 0x08);
				((S64*)result)[2] = 0;
				{
					void* src = *(void**)((U8*)me_ + 0x10);
					while (src != NULL)
					{
						U8* node = (U8*)AllocMem(0x08 + (size_t)size);
						if (is_ref)
						{
							if (*(void**)((U8*)src + 0x08) == NULL)
								*(void**)(node + 0x08) = NULL;
							else
								*(void**)(node + 0x08) = _copy(*(void**)((U8*)src + 0x08), type + 1);
						}
						else
							memcpy(node + 0x08, (U8*)src + 0x08, (size_t)size);
						*(void**)node = NULL;
						if (top == NULL)
						{
							top = node;
							bottom = node;
						}
						else
							*(void**)bottom = node;
						src = *(void**)src;
					}
					*(void**)(result + 0x10) = top;
				}
				return result;
			}
		case TypeId_Queue:
			{
				int size = GetSize(type[1]);
				Bool is_ref = IsRef(type[1]);
				U8* result = (U8*)AllocMem(0x20);
				((S64*)result)[0] = DefaultRefCntOpe;
				((S64*)result)[1] = *(S64*)((U8*)me_ + 0x08);
				((S64*)result)[2] = 0;
				((S64*)result)[3] = 0;
				{
					void* src = *(void**)((U8*)me_ + 0x10);
					while (src != NULL)
					{
						U8* node = (U8*)AllocMem(0x08 + (size_t)size);
						if (is_ref)
						{
							if (*(void**)((U8*)src + 0x08) == NULL)
								*(void**)(node + 0x08) = NULL;
							else
								*(void**)(node + 0x08) = _copy(*(void**)((U8*)src + 0x08), type + 1);
						}
						else
							memcpy(node + 0x08, (U8*)src + 0x08, (size_t)size);
						*(void**)node = NULL;
						if (*(void**)(result + 0x18) == NULL)
							*(void**)(result + 0x10) = node;
						else
							*(void**)*(void**)(result + 0x18) = node;
						*(void**)(result + 0x18) = node;
						src = *(void**)src;
					}
				}
				return result;
			}
		case TypeId_Dict:
			{
				U8* child1;
				U8* child2;
				GetDictTypes(type, &child1, &child2);
				{
					U8* result = (U8*)AllocMem(0x18);
					((S64*)result)[0] = DefaultRefCntOpe;
					((S64*)result)[1] = *(S64*)((U8*)me_ + 0x08);
					((S64*)result)[2] = 0;
					if (*(void**)((U8*)me_ + 0x10) != NULL)
						*(void**)(result + 0x10) = CopyDictRecursion(*(void**)((U8*)me_ + 0x10), child1, child2);
					return result;
				}
			}
		default:
			ASSERT(*type == TypeId_Class);
			// TODO:
			break;
	}
}

EXPORT S64 _powInt(S64 n, S64 m)
{
	switch (m)
	{
		case 0:
			if (n == 0)
				THROW(0x1000, L"");
			return 1;
		case 1:
			return n;
		case 2:
			return Mul(n, n);
		default:
			{
				S64 result = 1;
				if (m < 0)
					THROW(0x1000, L"");
				while (m != 0)
				{
					if ((m & 1) == 1)
						result = Mul(result, n);
					m >>= 1;
					n = Mul(n, n);
				}
				return result;
			}
			break;
	}
}

EXPORT double _powFloat(double n, double m)
{
	return pow(n, m);
}

EXPORT double _mod(double n, double m)
{
	return fmod(n, m);
}

EXPORT S64 _cmpStr(const U8* a, const U8* b)
{
	return (S64)wcscmp((Char*)(a + 0x10), (Char*)(b + 0x10));
}

EXPORT void* _toBin(const void* me_, const U8* type)
{
	switch (*type)
	{
		case TypeId_Int:
			{
				U8* result = (U8*)AllocMem(0x10 + 0x08);
				*(S64*)(result + 0x10) = *(S64*)&me_;
				return result;
			}
		case TypeId_Float:
			{
				U8* result = (U8*)AllocMem(0x10 + 0x08);
				*(double*)(result + 0x10) = *(double*)&me_;
				return result;
			}
		case TypeId_Char:
			{
				U8* result = (U8*)AllocMem(0x10 + 0x02);
				*(Char*)(result + 0x10) = *(Char*)&me_;
				return result;
			}
		case TypeId_Bool:
			{
				U8* result = (U8*)AllocMem(0x10 + 0x01);
				*(Bool*)(result + 0x10) = *(Bool*)&me_;
				return result;
			}
		case TypeId_Bit8:
			{
				U8* result = (U8*)AllocMem(0x10 + 0x01);
				*(U8*)(result + 0x10) = *(U8*)&me_;
				return result;
			}
		case TypeId_Bit16:
			{
				U8* result = (U8*)AllocMem(0x10 + 0x02);
				*(U16*)(result + 0x10) = *(U16*)&me_;
				return result;
			}
		case TypeId_Bit32:
			{
				U8* result = (U8*)AllocMem(0x10 + 0x04);
				*(U32*)(result + 0x10) = *(U32*)&me_;
				return result;
			}
		case TypeId_Bit64:
			{
				U8* result = (U8*)AllocMem(0x10 + 0x08);
				*(U64*)(result + 0x10) = *(U64*)&me_;
				return result;
			}
		case TypeId_Array:
			{
				S64 len = *(S64*)((U8*)me_ + 0x08);
				void** bins = (void**)AllocMem(sizeof(void*) * (size_t)len);
				int size = GetSize(type[1]);
				U8* ptr = (U8*)me_ + 0x10;
				S64 i;
				for (i = 0; i < len; i++)
				{
					bins[i] = _toBin(ptr, type + 1);
					ptr += size;
				}
				return CatBin((int)len, bins);
			}
		case TypeId_List:
			{
				S64 len = *(S64*)((U8*)me_ + 0x08);
				void** bins = (void**)AllocMem(sizeof(void*) * (size_t)len);
				int size = GetSize(type[1]);
				void* ptr = *(void**)((U8*)me_ + 0x20);
				S64 i;
				for (i = 0; i < len; i++)
				{
					void* value = NULL;
					memcpy(&value, (U8*)ptr + 0x10, (size_t)size);
					bins[i] = _toBin(value, type + 1);
					ptr = *(void**)((U8*)ptr + 0x08);
				}
				return CatBin((int)len, bins);
			}
		case TypeId_Stack:
		case TypeId_Queue:
			{
				S64 len = *(S64*)((U8*)me_ + 0x08);
				void** bins = (void**)AllocMem(sizeof(void*) * (size_t)len);
				int size = GetSize(type[1]);
				void* ptr = *(void**)((U8*)me_ + 0x10);
				S64 i;
				for (i = 0; i < len; i++)
				{
					void* value = NULL;
					memcpy(&value, (U8*)ptr + 0x08, (size_t)size);
					bins[i] = _toBin(value, type + 1);
					ptr = *(void**)ptr;
				}
				return CatBin((int)len, bins);
			}
		case TypeId_Dict:
			{
				U8* child1;
				U8* child2;
				GetDictTypes(type, &child1, &child2);
				{
					S64 len = *(S64*)((U8*)me_ + 0x08);
					void** bins = (void**)AllocMem(sizeof(void*) * (size_t)len * 3); // 'key' + 'value' + 'info' per node.
					ToBinDictRecursion(bins, *(void**)((U8*)me_ + 0x10), child1, child2);
					return CatBin((int)len, bins);
				}
			}
			break;
		case TypeId_Func:
			THROW(0x1000, L"");
			return NULL;
		case TypeId_Enum:
			{
				U8* result = (U8*)AllocMem(0x10 + 0x08);
				*(S64*)(result + 0x10) = *(S64*)&me_;
				return result;
			}
		default:
			ASSERT(*type == TypeId_Class);
			// TODO:
			break;
	}
}

EXPORT S64 _fromBin(void** me_, const U8* type, const U8* bin, S64 idx)
{
	switch (*type)
	{
		case TypeId_Int:
			*(S64*)me_ = *(S64*)(bin + idx);
			return idx + 8;
		case TypeId_Float:
			*(float*)me_ = *(float*)(bin + idx);
			return idx + 8;
		case TypeId_Char:
			*(Char*)me_ = *(Char*)(bin + idx);
			return idx + 2;
		case TypeId_Bool:
			*(Bool*)me_ = *(Bool*)(bin + idx);
			return idx + 1;
		case TypeId_Bit8:
			*(U8*)me_ = *(U8*)(bin + idx);
			return idx + 1;
		case TypeId_Bit16:
			*(U16*)me_ = *(U16*)(bin + idx);
			return idx + 2;
		case TypeId_Bit32:
			*(U32*)me_ = *(U32*)(bin + idx);
			return idx + 4;
		case TypeId_Bit64:
			*(U64*)me_ = *(U64*)(bin + idx);
			return idx + 8;
		case TypeId_Array:
			{
				S64 len = *(S64*)(bin + idx);
				idx += 8;
				{
					int size = GetSize(type[1]);
					void* result = AllocMem(0x10 + (size_t)len * (size_t)size);
					((S64*)result)[0] = DefaultRefCntFunc;
					((S64*)result)[1] = len;
					{
						U8* ptr = (U8*)result + 0x10;
						S64 i;
						for (i = 0; i < len; i++)
						{
							void* value = NULL;
							idx = _fromBin(&value, type + 1, bin, idx);
							memcpy(ptr, value, (size_t)size);
							ptr += size;
						}
						*me_ = result;
					}
				}
				return idx;
			}
		case TypeId_List:
			{
				S64 len = *(S64*)(bin + idx);
				idx += 8;
				{
					int size = GetSize(type[1]);
					void* result = AllocMem(0x28);
					((S64*)result)[0] = DefaultRefCntFunc;
					((S64*)result)[1] = len;
					((S64*)result)[2] = 0;
					((S64*)result)[3] = 0;
					((S64*)result)[4] = 0;
					{
						S64 i;
						for (i = 0; i < len; i++)
						{
							U8* node = (U8*)AllocMem(0x10 + size);
							void* value = NULL;
							idx = _fromBin(&value, type + 1, bin, idx);
							memcpy(node + 0x10, value, (size_t)size);
							*(void**)(node + 0x08) = NULL;
							if (*(void**)((U8*)result + 0x10) == NULL)
							{
								*(void**)node = NULL;
								*(void**)((U8*)result + 0x10) = node;
								*(void**)((U8*)result + 0x18) = node;
							}
							else
							{
								*(void**)node = *(void**)((U8*)result + 0x18);
								*(void**)((U8*)*(void**)((U8*)result + 0x18) + 0x08) = node;
								*(void**)((U8*)result + 0x18) = node;
							}
						}
					}
				}
				return idx;
			}
		case TypeId_Stack:
			{
				S64 len = *(S64*)(bin + idx);
				idx += 8;
				{
					int size = GetSize(type[1]);
					void* result = AllocMem(0x18);
					((S64*)result)[0] = DefaultRefCntFunc;
					((S64*)result)[1] = len;
					((S64*)result)[2] = 0;
					{
						S64 i;
						for (i = 0; i < len; i++)
						{
							U8* node = (U8*)AllocMem(0x08 + size);
							void* value = NULL;
							idx = _fromBin(&value, type + 1, bin, idx);
							memcpy(node + 0x10, value, (size_t)size);
							*(void**)node = *(void**)((U8*)result + 0x10);
							*(void**)((U8*)result + 0x10) = node;
						}
					}
				}
				return idx;
			}
		case TypeId_Queue:
			{
				S64 len = *(S64*)(bin + idx);
				idx += 8;
				{
					int size = GetSize(type[1]);
					void* result = AllocMem(0x20);
					((S64*)result)[0] = DefaultRefCntFunc;
					((S64*)result)[1] = len;
					((S64*)result)[2] = 0;
					((S64*)result)[3] = 0;
					{
						S64 i;
						for (i = 0; i < len; i++)
						{
							U8* node = (U8*)AllocMem(0x08 + size);
							void* value = NULL;
							idx = _fromBin(&value, type + 1, bin, idx);
							memcpy(node + 0x10, value, (size_t)size);
							*(void**)node = NULL;
							if (*(void**)((U8*)result + 0x18) == NULL)
								*(void**)((U8*)result + 0x10) = node;
							else
								*(void**)*(void**)((U8*)result + 0x18) = node;
							*(void**)((U8*)result + 0x18) = node;
						}
					}
				}
				return idx;
			}
		case TypeId_Dict:
			{
				U8* child1;
				U8* child2;
				GetDictTypes(type, &child1, &child2);
				{
					S64 len = *(S64*)(bin + idx);
					idx += 8;
					{
						void* result = AllocMem(0x18);
						((S64*)result)[0] = DefaultRefCntFunc;
						((S64*)result)[1] = len;
						((S64*)result)[2] = 0;
						if (len != 0)
							*(void**)((U8*)result + 0x10) = FromBinDictRecursion(child1, child2, bin, &idx);
					}
				}
				return idx;
			}
		case TypeId_Func:
			THROW(0x1000, NULL);
			return idx;
		case TypeId_Enum:
			*(S64*)me_ = *(S64*)(bin + idx);
			return idx + 8;
		default:
			ASSERT(*type == TypeId_Class);
			// TODO:
			break;
	}
}

EXPORT U8* _toStr(const void* me_, const U8* type)
{
	Char str[33];
	int len;
	switch (*type)
	{
		case TypeId_Int: len = swprintf(str, 33, L"%I64d", *(S64*)&me_); break;
		case TypeId_Float: len = swprintf(str, 33, L"%g", *(double*)&me_); break;
		case TypeId_Char:
			len = 1;
			str[0] = *(Char*)&me_;
			str[1] = L'\0';
			break;
		case TypeId_Bool:
			if (*(Bool*)&me_)
			{
				len = 4;
				wcscpy(str, L"true");
			}
			else
			{
				len = 5;
				wcscpy(str, L"false");
			}
			break;
		case TypeId_Bit8: len = swprintf(str, 33, L"0x%02X", *(U8*)&me_); break;
		case TypeId_Bit16: len = swprintf(str, 33, L"0x%04X", *(U16*)&me_); break;
		case TypeId_Bit32: len = swprintf(str, 33, L"0x%08X", *(U32*)&me_); break;
		case TypeId_Bit64: len = swprintf(str, 33, L"0x%016I64X", *(U64*)&me_); break;
		default:
			ASSERT(False);
			break;
	}
	ASSERT(len < 33);
	{
		U8* result = (U8*)AllocMem(0x10 + sizeof(Char) * (size_t)(len + 1));
		((S64*)result)[0] = DefaultRefCntFunc;
		((S64*)result)[1] = (S64)len;
		memcpy(result + 0x10, str, sizeof(Char) * (size_t)(len + 1));
		return result;
	}
}

EXPORT S64 _or(const void* me_, const U8* type, const void* n)
{
	switch (*type)
	{
		case TypeId_Bit8: return (S64)(U64)(*(U8*)&me_ | *(U8*)&n);
		case TypeId_Bit16: return (S64)(U64)(*(U16*)&me_ | *(U16*)&n);
		case TypeId_Bit32: return (S64)(U64)(*(U32*)&me_ | *(U32*)&n);
		case TypeId_Bit64: return (S64)(*(U64*)&me_ | *(U64*)&n);
		default:
			ASSERT(False);
			return 0;
	}
}

EXPORT S64 _and(const void* me_, const U8* type, const void* n)
{
	switch (*type)
	{
		case TypeId_Bit8: return (S64)(U64)(*(U8*)&me_ & *(U8*)&n);
		case TypeId_Bit16: return (S64)(U64)(*(U16*)&me_ & *(U16*)&n);
		case TypeId_Bit32: return (S64)(U64)(*(U32*)&me_ & *(U32*)&n);
		case TypeId_Bit64: return (S64)(*(U64*)&me_ & *(U64*)&n);
		default:
			ASSERT(False);
			return 0;
	}
}

EXPORT S64 _xor(const void* me_, const U8* type, const void* n)
{
	switch (*type)
	{
		case TypeId_Bit8: return (S64)(U64)(*(U8*)&me_ ^ *(U8*)&n);
		case TypeId_Bit16: return (S64)(U64)(*(U16*)&me_ ^ *(U16*)&n);
		case TypeId_Bit32: return (S64)(U64)(*(U32*)&me_ ^ *(U32*)&n);
		case TypeId_Bit64: return (S64)(*(U64*)&me_ ^ *(U64*)&n);
		default:
			ASSERT(False);
			return 0;
	}
}

EXPORT S64 _not(const void* me_, const U8* type)
{
	switch (*type)
	{
		case TypeId_Bit8: return (S64)(U64)(~*(U8*)&me_);
		case TypeId_Bit16: return (S64)(U64)(~*(U16*)&me_);
		case TypeId_Bit32: return (S64)(U64)(~*(U32*)&me_);
		case TypeId_Bit64: return (S64)(~*(U64*)&me_);
		default:
			ASSERT(False);
			return 0;
	}
}

EXPORT S64 _shl(const void* me_, const U8* type, S64 n)
{
	switch (*type)
	{
		case TypeId_Bit8: return (S64)(U64)(*(U8*)&me_ << n);
		case TypeId_Bit16: return (S64)(U64)(*(U16*)&me_ << n);
		case TypeId_Bit32: return (S64)(U64)(*(U32*)&me_ << n);
		case TypeId_Bit64: return (S64)(*(U64*)&me_ << n);
		default:
			ASSERT(False);
			return 0;
	}
}

EXPORT S64 _shr(const void* me_, const U8* type, S64 n)
{
	// In Visual C++, shifting of unsigned type is guaranteed to be a logical shift.
	switch (*type)
	{
		case TypeId_Bit8: return (S64)(U64)(*(U8*)&me_ >> n);
		case TypeId_Bit16: return (S64)(U64)(*(U16*)&me_ >> n);
		case TypeId_Bit32: return (S64)(U64)(*(U32*)&me_ >> n);
		case TypeId_Bit64: return (S64)(*(U64*)&me_ >> n);
		default:
			ASSERT(False);
			return 0;
	}
}

EXPORT S64 _sar(const void* me_, const U8* type, S64 n)
{
	// In Visual C++, shifting of signed type is guaranteed to be an arithmetic shift.
	switch (*type)
	{
		case TypeId_Bit8: return (S64)((S8)*(U8*)&me_ >> n);
		case TypeId_Bit16: return (S64)((S16)*(U16*)&me_ >> n);
		case TypeId_Bit32: return (S64)((S32)*(U32*)&me_ >> n);
		case TypeId_Bit64: return ((S64)*(U64*)&me_ >> n);
		default:
			ASSERT(False);
			return 0;
	}
}

EXPORT void* _sub(const void* me_, const U8* type, S64 start, S64 len)
{
	S64 len2 = *(S64*)((U8*)me_ + 0x08);
	if (len == -1)
		len = len2 - start;
	if (start < 0 || len < 0 || start + len > len2)
		THROW(0x1000, L"");
	{
		Bool is_str = IsStr(type);
		int size = GetSize(type[1]);
		U8* result = (U8*)AllocMem(0x10 + (size_t)(size * (len + (is_str ? 1 : 0))));
		const U8* src = (U8*)me_ + 0x10 + size * start;
		((S64*)result)[0] = DefaultRefCntFunc;
		((S64*)result)[1] = len;
		memcpy(result + 0x10, src, (size_t)(size * len));
		if (IsRef(type[1]))
		{
			void** ptr = (void**)(result + 0x10);
			S64 i;
			for (i = 0; i < len; i++)
			{
				if (*ptr != NULL)
					(*(S64*)*ptr)++;
				ptr++;
			}
		}
		if (is_str)
			*(Char*)(result + 0x10 + (size * len)) = L'\0';
		return result;
	}
}

EXPORT void _reverse(void* me_, const U8* type)
{
	int size = GetSize(*type);
	S64 len = *(S64*)((U8*)me_ + 0x08);
	// TODO:
}

EXPORT void _shuffle(void* me_, const U8* type)
{
	int size = GetSize(*type);
	S64 len = *(S64*)((U8*)me_ + 0x08);
	// TODO:
}

EXPORT void _sortAsc(void* me_, const U8* type)
{
	int size = GetSize(*type);
	int(*cmp)(const void* a, const void* b) = GetCmpFunc(type + 1);
	if (cmp == NULL)
		Throw(0x1000, L"");
	{
		S64 len = *(S64*)((U8*)me_ + 0x08);
		// TODO:
	}
}

EXPORT void _sortDesc(void* me_, const U8* type)
{
	int size = GetSize(*type);
	int(*cmp)(const void* a, const void* b) = GetCmpFunc(type + 1);
	if (cmp == NULL)
		Throw(0x1000, L"");
	{
		S64 len = *(S64*)((U8*)me_ + 0x08);
		// TODO:
	}
}

EXPORT S64 _find(const void* me_, const U8* type, const void* item)
{
	int size = GetSize(type[1]);
	int(*cmp)(const void* a, const void* b) = GetCmpFunc(type + 1);
	if (cmp == NULL)
		Throw(0x1000, L"");
	{
		S64 len = *(S64*)((U8*)me_ + 0x08);
		U8* ptr = (U8*)me_ + 0x10;
		S64 i;
		for (i = 0; i < len; i++)
		{
			void* value = NULL;
			memcpy(&value, ptr, size);
			if (cmp(value, item) == 0)
				return i;
			ptr += size;
		}
	}
	return -1;
}

EXPORT S64 _findLast(const void* me_, const U8* type, const void* item)
{
	int size = GetSize(type[1]);
	int(*cmp)(const void* a, const void* b) = GetCmpFunc(type + 1);
	if (cmp == NULL)
		Throw(0x1000, L"");
	{
		S64 len = *(S64*)((U8*)me_ + 0x08);
		U8* ptr = (U8*)me_ + 0x10 + (size_t)size * (size_t)(len - 1);
		S64 i;
		for (i = len - 1; i >= 0; i--)
		{
			void* value = NULL;
			memcpy(&value, ptr, size);
			if (cmp(value, item) == 0)
				return i;
			ptr -= size;
		}
		return -1;
	}
}

EXPORT S64 _toInt(const U8* me_)
{
	S64 result;
	errno = 0;
	result = _wtoi64((const Char*)(me_ + 0x10));
	if (errno != 0)
		THROW(0x1000, L"");
	return result;
}

EXPORT double _toFloat(const U8* me_)
{
	double result;
	errno = 0;
	result = _wtof((const Char*)(me_ + 0x10));
	if (errno != 0)
		THROW(0x1000, L"");
	return result;
}

EXPORT void* _lower(const U8* me_)
{
	S64 len = *(S64*)(me_ + 0x08);
	U8* result = (U8*)AllocMem(0x10 + sizeof(Char) * (size_t)(len + 1));
	((S64*)result)[0] = DefaultRefCntFunc;
	((S64*)result)[1] = len;
	{
		Char* dst = (Char*)(result + 0x10);
		const Char* src = (Char*)(me_ + 0x10);
		while (len > 0)
		{
			if (L'A' <= *src && *src <= L'Z')
				*dst = *src - L'A' + L'a';
			else
				*dst = *src;
			dst++;
			src++;
			len--;
		}
		*dst = L'\0';
	}
	return result;
}

EXPORT void* _upper(const U8* me_)
{
	S64 len = *(S64*)(me_ + 0x08);
	U8* result = (U8*)AllocMem(0x10 + sizeof(Char) * (size_t)(len + 1));
	((S64*)result)[0] = DefaultRefCntFunc;
	((S64*)result)[1] = len;
	{
		Char* dst = (Char*)(result + 0x10);
		const Char* src = (Char*)(me_ + 0x10);
		while (len > 0)
		{
			if (L'a' <= *src && *src <= L'z')
				*dst = *src - L'a' + L'A';
			else
				*dst = *src;
			dst++;
			src++;
			len--;
		}
		*dst = L'\0';
	}
	return result;
}

EXPORT void* _trim(const U8* me_)
{
	S64 len = *(S64*)(me_ + 0x08);
	const Char* begin = (Char*)(me_ + 0x10);
	const Char* end = begin + len - 1;
	while (len > 0 && IsSpace(*begin))
	{
		begin++;
		len--;
	}
	while (len > 0 && IsSpace(*end))
	{
		end--;
		len--;
	}
	{
		U8* result = (U8*)AllocMem(0x10 + sizeof(Char) * (size_t)(len + 1));
		((S64*)result)[0] = DefaultRefCntFunc;
		((S64*)result)[1] = len;
		ASSERT(len == end - begin + 1);
		memcpy(result + 0x10, begin, sizeof(Char) * (size_t)len);
		*(Char*)(result + 0x10 + sizeof(Char) * (size_t)len) = L'\0';
		return result;
	}
}

EXPORT void* _trimLeft(const U8* me_)
{
	S64 len = *(S64*)(me_ + 0x08);
	const Char* begin = (Char*)(me_ + 0x10);
	while (len > 0 && IsSpace(*begin))
	{
		begin++;
		len--;
	}
	{
		U8* result = (U8*)AllocMem(0x10 + sizeof(Char) * (size_t)(len + 1));
		((S64*)result)[0] = DefaultRefCntFunc;
		((S64*)result)[1] = len;
		memcpy(result + 0x10, begin, sizeof(Char) * (size_t)len);
		*(Char*)(result + 0x10 + sizeof(Char) * (size_t)len) = L'\0';
		return result;
	}
}

EXPORT void* _trimRight(const U8* me_)
{
	S64 len = *(S64*)(me_ + 0x08);
	const Char* end = (Char*)(me_ + 0x10) + len - 1;
	while (len > 0 && IsSpace(*end))
	{
		end--;
		len--;
	}
	{
		U8* result = (U8*)AllocMem(0x10 + sizeof(Char) * (size_t)(len + 1));
		((S64*)result)[0] = DefaultRefCntFunc;
		((S64*)result)[1] = len;
		memcpy(result + 0x10, me_ + 0x10, sizeof(Char) * (size_t)len);
		*(Char*)(result + 0x10 + sizeof(Char) * (size_t)len) = L'\0';
		return result;
	}
}

EXPORT void _addList(void* me_, const U8* type, const void* item)
{
	int item_size = GetSize(type[1]);
	U8* node = (U8*)AllocMem(0x10 + item_size);
	Copy(node + 0x10, type[1], &item);
	*(void**)(node + 0x08) = NULL;
	if (*(void**)((U8*)me_ + 0x10) == NULL)
	{
		*(void**)node = NULL;
		*(void**)((U8*)me_ + 0x10) = node;
		*(void**)((U8*)me_ + 0x18) = node;
	}
	else
	{
		*(void**)node = *(void**)((U8*)me_ + 0x18);
		*(void**)((U8*)(*(void**)((U8*)me_ + 0x18)) + 0x08) = node;
		*(void**)((U8*)me_ + 0x18) = node;
	}
	(*(S64*)((U8*)me_ + 0x08))++;
}

EXPORT void _addDict(void* me_, const U8* type, const U8* value_type, const void* key, const void* item)
{
	Bool addition;
	U8* child1;
	U8* child2;
	UNUSED(value_type);
	GetDictTypes(type, &child1, &child2);
	*(void**)((U8*)me_ + 0x10) = AddDictRecursion(*(void**)((U8*)me_ + 0x10), key, item, GetCmpFunc(child1), child1, child2, &addition);
	if (addition)
		(*(S64*)((U8*)me_ + 0x08))++;
	*(Bool*)((U8*)*(void**)((U8*)me_ + 0x10) + 0x10) = False;
}

EXPORT void* _getList(void* me_, const U8* type)
{
	void* result = NULL;
	Copy(&result, type[1], (U8*)*(void**)((U8*)me_ + 0x20) + 0x10);
	return result;
}

EXPORT void* _getDict(void* me_, const U8* type, const void* key)
{
	U8* child1;
	U8* child2;
	GetDictTypes(type, &child1, &child2);
	{
		int(*cmp_func)(const void* a, const void* b) = GetCmpFunc(child1);
		const void* node = *(void**)((U8*)me_ + 0x10);
		while (node != NULL)
		{
			int cmp = cmp_func(key, *(void**)((U8*)node + 0x18));
			if (cmp == 0)
			{
				void* result = NULL;
				Copy(&result, *child2, (U8*)node + 0x20);
				return result;
			}
			if (cmp < 0)
				node = *(void**)node;
			else
				node = *(void**)((U8*)node + 0x08);
		}
	}
	return NULL;
}

EXPORT void _head(void* me_, const U8* type)
{
	UNUSED(type);
	*(void**)((U8*)me_ + 0x20) = *(void**)((U8*)me_ + 0x10);
}

EXPORT void _tail(void* me_, const U8* type)
{
	UNUSED(type);
	*(void**)((U8*)me_ + 0x20) = *(void**)((U8*)me_ + 0x18);
}

EXPORT void _next(void* me_, const U8* type)
{
	void* ptr = *(void**)((U8*)me_ + 0x20);
	UNUSED(type);
	*(void**)((U8*)me_ + 0x20) = *(void**)((U8*)ptr + 0x08);
}

EXPORT void _prev(void* me_, const U8* type)
{
	void* ptr = *(void**)((U8*)me_ + 0x20);
	UNUSED(type);
	*(void**)((U8*)me_ + 0x20) = *(void**)ptr;
}

EXPORT Bool _end(void* me_, const U8* type)
{
	UNUSED(type);
	return *(void**)((U8*)me_ + 0x20) == 0;
}

EXPORT void _del(void* me_, const U8* type)
{
	if (*(void**)((U8*)me_ + 0x20) == NULL)
		THROW(0x1000, L"");
	{
		void* ptr = *(void**)((U8*)me_ + 0x20);
		void* next = *(void**)((U8*)ptr + 0x08);
		void* prev = *(void**)ptr;
		if (prev == NULL)
			*(void**)((U8*)me_ + 0x10) = next;
		else
			*(void**)((U8*)prev + 0x08) = next;
		if (next == NULL)
			*(void**)((U8*)me_ + 0x18) = prev;
		else
			*(void**)next = prev;
		*(void**)((U8*)me_ + 0x20) = next;
		(*(S64*)((U8*)me_ + 0x08))--;
		if (IsRef(type[1]))
		{
			void* ptr2 = *(void**)((U8*)ptr + 0x10);
			(*(S64*)ptr2)--;
			if (*(S64*)ptr2 == 0)
				_freeSet(ptr2, type + 1);
		}
		FreeMem(ptr);
	}
}

EXPORT void _ins(void* me_, const U8* type, const void* item)
{
	if (*(void**)((U8*)me_ + 0x20) == NULL)
		THROW(0x1000, L"");
	{
		void* ptr = *(void**)((U8*)me_ + 0x20);
		int item_size = GetSize(type[1]);
		U8* node = (U8*)AllocMem(0x10 + item_size);
		Copy(node + 0x10, type[1], &item);
		if (*(void**)ptr == NULL)
			*(void**)((U8*)me_ + 0x10) = node;
		else
			*(void**)((U8*)*(void**)ptr + 0x08) = node;
		*(void**)((U8*)node + 0x08) = ptr;
		*(void**)node = *(void**)ptr;
		*(void**)ptr = node;
		(*(S64*)((U8*)me_ + 0x08))++;
	}
}

EXPORT void* _peek(void* me_, const U8* type)
{
	void* node = *(void**)((U8*)me_ + 0x10);
	void* result = NULL;
	Copy(&result, type[1], (U8*)node + 0x08);
	return result;
}

EXPORT void _push(void* me_, const U8* type, const void* item)
{
	int item_size = GetSize(type[1]);
	U8* node = (U8*)AllocMem(0x08 + item_size);
	Copy(node + 0x08, type[1], &item);
	*(void**)node = *(void**)((U8*)me_ + 0x10);
	*(void**)((U8*)me_ + 0x10) = node;
	(*(S64*)((U8*)me_ + 0x08))++;
}

EXPORT void* _pop(void* me_, const U8* type)
{
	void* node = *(void**)((U8*)me_ + 0x10);
	void* result = NULL;
	Copy(&result, type[1], (U8*)node + 0x08);
	*(void**)((U8*)me_ + 0x10) = *(void**)node;
	(*(S64*)((U8*)me_ + 0x08))--;
	if (IsRef(type[1]))
	{
		void* ptr2 = *(void**)((U8*)node + 0x08);
		(*(S64*)ptr2)--;
		if (*(S64*)ptr2 == 0)
			_freeSet(ptr2, type + 1);
	}
	FreeMem(node);
	return result;
}

EXPORT void _enq(void* me_, const U8* type, const void* item)
{
	int item_size = GetSize(type[1]);
	U8* node = (U8*)AllocMem(0x08 + item_size);
	Copy(node + 0x08, type[1], &item);
	*(void**)node = NULL;
	if (*(void**)((U8*)me_ + 0x18) == NULL)
		*(void**)((U8*)me_ + 0x10) = node;
	else
		*(void**)*(void**)((U8*)me_ + 0x18) = node;
	*(void**)((U8*)me_ + 0x18) = node;
	(*(S64*)((U8*)me_ + 0x08))++;
}

EXPORT void* _deq(void* me_, const U8* type)
{
	void* node = *(void**)((U8*)me_ + 0x10);
	void* result = NULL;
	Copy(&result, type[1], (U8*)node + 0x08);
	*(void**)((U8*)me_ + 0x10) = *(void**)node;
	if (*(void**)node == NULL)
		*(void**)((U8*)me_ + 0x18) = NULL;
	(*(S64*)((U8*)me_ + 0x08))--;
	if (IsRef(type[1]))
	{
		void* ptr2 = *(void**)((U8*)node + 0x08);
		(*(S64*)ptr2)--;
		if (*(S64*)ptr2 == 0)
			_freeSet(ptr2, type + 1);
	}
	FreeMem(node);
	return result;
}

static Bool IsRef(U8 type)
{
	return type == TypeId_Array || type == TypeId_List || type == TypeId_Stack || type == TypeId_Queue || type == TypeId_Dict || type == TypeId_Class;
}

static int GetSize(U8 type)
{
	switch (type)
	{
		case TypeId_Int: return 8;
		case TypeId_Float: return 8;
		case TypeId_Char: return 2;
		case TypeId_Bool: return 1;
		case TypeId_Bit8: return 1;
		case TypeId_Bit16: return 2;
		case TypeId_Bit32: return 4;
		case TypeId_Bit64: return 8;
		case TypeId_Array: return 8;
		case TypeId_List: return 8;
		case TypeId_Stack: return 8;
		case TypeId_Queue: return 8;
		case TypeId_Dict: return 8;
		case TypeId_Func: return 8;
		case TypeId_Enum: return 8;
		case TypeId_Class: return 8;
	}
	ASSERT(False);
	return 0;
}

static Bool Same(double f1, double f2)
{
	U64 i1 = *(U64*)&f1;
	U64 i2 = *(U64*)&f2;
	U64 diff;
	if ((i1 >> 63) != (i2 >> 63))
		return f1 == f2;
	diff = i1 - i2;
	return -24 <= diff && diff <= 24;
}

static S64 Add(S64 a, S64 b)
{
	S64 result = a + b;
#if defined(DBG)
	if (Same((double)result, (double)a + (double)b))
		THROW(0x1000, L"");
#endif
	return result;
}

static S64 Mul(S64 a, S64 b)
{
	S64 result = a * b;
#if defined(DBG)
	if (Same((double)result, (double)a * (double)b))
		THROW(0x1000, L"");
#endif
	return result;
}

static void GetDictTypes(const U8* type, const U8** child1, const U8** child2)
{
	const U8* type2 = type;
	type2++;
	*child1 = type2;
	GetDictTypesRecursion(&type2);
	type2++;
	*child2 = type2;
}

static void GetDictTypesRecursion(const U8** type)
{
	switch (**type)
	{
		case TypeId_Int:
		case TypeId_Float:
		case TypeId_Char:
		case TypeId_Bool:
		case TypeId_Bit8:
		case TypeId_Bit16:
		case TypeId_Bit32:
		case TypeId_Bit64:
		case TypeId_Func:
		case TypeId_Enum:
		case TypeId_Class:
			break;
		case TypeId_Array:
		case TypeId_List:
		case TypeId_Stack:
		case TypeId_Queue:
			(*type)++;
			GetDictTypesRecursion(type);
			break;
		case TypeId_Dict:
			(*type)++;
			GetDictTypesRecursion(type);
			(*type)++;
			GetDictTypesRecursion(type);
			(*type)++;
			break;
		default:
			ASSERT(False);
	}
}

static void FreeDict(void* ptr, const U8* child1, const U8* child2)
{
	if (*(void**)ptr != NULL)
		FreeDict(*(void**)ptr, child1, child2);
	if (*(void**)((U8*)ptr + 0x08) != NULL)
		FreeDict(*(void**)((U8*)ptr + 0x08), child1, child2);
	if (IsRef(*child1))
	{
		void* ptr2 = *(void**)((U8*)ptr + 0x18);
		(*(S64*)ptr2)--;
		if (*(S64*)ptr2 == 0)
			_freeSet(ptr2, child1);
	}
	if (IsRef(*child2))
	{
		void* ptr2 = *(void**)((U8*)ptr + 0x20);
		(*(S64*)ptr2)--;
		if (*(S64*)ptr2 == 0)
			_freeSet(ptr2, child2);
	}
	FreeMem(ptr);
}

static Bool IsStr(const U8* type)
{
	return type[0] == TypeId_Array && type[1] == TypeId_Char;
}

static Bool IsSpace(Char c)
{
	return 0x09 <= c && c <= 0x0d || c == L' ' || c == 0xa0;
}

static void Copy(void* dst, U8 type, const void* src)
{
	switch (GetSize(type))
	{
		case 1: *(U8*)dst = *(const U8*)src; break;
		case 2: *(U16*)dst = *(const U16*)src; break;
		case 4: *(U32*)dst = *(const U32*)src; break;
		case 8: *(U64*)dst = *(const U64*)src; break;
		default:
			ASSERT(False);
			break;
	}
	if (IsRef(type) && *(void**)dst != NULL)
		(*(S64*)*(void**)dst)++;
}

static void* AddDictRecursion(void* node, const void* key, const void* item, int cmp_func(const void* a, const void* b), U8* key_type, U8* item_type, Bool* addition)
{
	if (node == NULL)
	{
		void* n = AllocMem(0x20 + GetSize(*item_type));
		*(void**)((U8*)n + 0x18) = NULL;
		Copy((U8*)n + 0x18, *key_type, &key);
		Copy((U8*)n + 0x20, *item_type, &item);
		*(U64*)((U8*)n + 0x10) = (U64)True;
		*(void**)n = NULL;
		*(void**)((U8*)n + 0x08) = NULL;
		*addition = True;
		return n;
	}
	{
		int cmp = cmp_func(key, *(void**)((U8*)node + 0x18));
		if (cmp == 0)
		{
			if (IsRef(*item_type) && *(const void**)((U8*)node + 0x20) != NULL)
				_freeSet((U8*)node + 0x20, item_type);
			*(const void**)((U8*)node + 0x20) = item;
			*addition = False;
			return node;
		}
		if (cmp < 0)
			*(void**)node = AddDictRecursion(*(void**)node, key, item, cmp_func, key_type, item_type, addition);
		else
			*(void**)((U8*)node + 0x08) = AddDictRecursion(*(void**)((U8*)node + 0x08), key, item, cmp_func, key_type, item_type, addition);
	}
	if (*(void**)((U8*)node + 0x08) != NULL && *(Bool*)((U8*)*(void**)((U8*)node + 0x08) + 0x10))
	{
		void* r = *(void**)((U8*)node + 0x08);
		*(void**)((U8*)node + 0x08) = *(void**)r;
		*(void**)r = node;
		*(Bool*)((U8*)r + 0x10) = *(Bool*)((U8*)node + 0x10);
		*(Bool*)((U8*)node + 0x10) = True;
		node = r;
	}
	if (*(void**)node != NULL && *(Bool*)((U8*)*(void**)node + 0x10) && *(void**)*(void**)node != NULL && *(Bool*)((U8*)*(void**)*(void**)node + 0x10))
	{
		void* l = *(void**)node;
		*(void**)node = *(void**)((U8*)l + 0x08);
		*(void**)((U8*)l + 0x08) = node;
		*(Bool*)((U8*)l + 0x10) = *(Bool*)((U8*)node + 0x10);
		*(Bool*)((U8*)node + 0x10) = True;
		node = l;
		*(Bool*)((U8*)node + 0x10) = True;
		*(Bool*)((U8*)*(void**)node + 0x10) = False;
		*(Bool*)((U8*)*(void**)((U8*)node + 0x08) + 0x10) = False;
	}
	// '*addition' should have been set so far.
	return node;
}

static void* CopyDictRecursion(void* node, U8* key_type, U8* item_type)
{
	U8* result = (U8*)AllocMem(0x20 + GetSize(*item_type));
	if (*(void**)node == NULL)
		*(void**)result = NULL;
	else
		*(void**)result = CopyDictRecursion(*(void**)node, key_type, item_type);
	if (*(void**)((U8*)node + 0x08) == NULL)
		*(void**)(result + 0x08) = NULL;
	else
		*(void**)(result + 0x08) = CopyDictRecursion(*(void**)((U8*)node + 0x08), key_type, item_type);
	*(void**)(result + 0x10) = *(void**)((U8*)node + 0x10);
	if (IsRef(*key_type))
	{
		if (*(void**)((U8*)node + 0x18) == NULL)
			*(void**)(result + 0x18) = NULL;
		else
			*(void**)(result + 0x18) = _copy(*(void**)((U8*)node + 0x18), key_type);
	}
	else
	{
		*(void**)(result + 0x18) = NULL;
		memcpy(result + 0x18, (U8*)node + 0x18, GetSize(*key_type));
	}
	if (IsRef(*item_type))
	{
		if (*(void**)((U8*)node + 0x20) == NULL)
			*(void**)(result + 0x20) = NULL;
		else
			*(void**)(result + 0x20) = _copy(*(void**)((U8*)node + 0x20), item_type);
	}
	else
		memcpy(result + 0x20, (U8*)node + 0x20, GetSize(*item_type));
	return result;
}

static void ToBinDictRecursion(void** buf, void* node, U8* key_type, U8* item_type)
{
	{
		void* key = NULL;
		memcpy(&key, (U8*)node + 0x18, GetSize(key_type[1]));
		*buf = _toBin(key, key_type + 1);
		buf++;
	}
	{
		void* value = NULL;
		memcpy(&value, (U8*)node + 0x20, GetSize(item_type[1]));
		*buf = _toBin(value, item_type + 1);
		buf++;
	}
	{
		U8* info = (U8*)AllocMem(0x11);
		((S64*)info)[0] = DefaultRefCntFunc;
		((S64*)info)[1] = 1;
		{
			U8 flag = 0;
			if (*(void**)node != NULL)
				flag |= 0x01;
			if (*(void**)((U8*)node + 0x08) != NULL)
				flag |= 0x02;
			if ((Bool)*(S64*)((U8*)node + 0x10))
				flag |= 0x04;
			*(info + 0x10) = flag;
		}
		*buf = info;
		buf++;
	}
	if (*(void**)node != NULL)
		ToBinDictRecursion(buf, *(void**)node, key_type, item_type);
	if (*(void**)((U8*)node + 0x08) != NULL)
		ToBinDictRecursion(buf, *(void**)((U8*)node + 0x08), key_type, item_type);
}

static void* FromBinDictRecursion(U8* key_type, U8* item_type, const U8* bin, S64* idx)
{
	U8* node = (U8*)AllocMem(0x20 + GetSize(*item_type));
	{
		void* key = NULL;
		*idx = _fromBin(&key, key_type, bin, *idx);
		*(void**)(node + 0x18) = NULL;
		memcpy(node + 0x18, &key, GetSize(*key_type));
	}
	{
		void* value = NULL;
		*idx = _fromBin(&value, item_type, bin, *idx);
		memcpy(node + 0x20, &value, GetSize(*item_type));
	}
	{
		U8 info = *(bin + *idx);
		idx++;
		if ((info & 0x01) != 0)
			*(void**)node = FromBinDictRecursion(key_type, item_type, bin, idx);
		else
			*(void**)node = NULL;
		if ((info & 0x02) != 0)
			*(void**)(node + 0x08) = FromBinDictRecursion(key_type, item_type, bin, idx);
		else
			*(void**)(node + 0x08) = NULL;
		*(S64*)(node + 0x10) = (info & 0x04) != 0 ? 1 : 0;
	}
	return node;
}

static int(*GetCmpFunc(const U8* type))(const void* a, const void* b)
{
	switch (*type)
	{
		case TypeId_Int: return CmpInt;
		case TypeId_Float: return CmpFloat;
		case TypeId_Char: return CmpChar;
		case TypeId_Bit8: return CmpBit8;
		case TypeId_Bit16: return CmpBit16;
		case TypeId_Bit32: return CmpBit32;
		case TypeId_Bit64: return CmpBit64;
		case TypeId_Array:
			if (type[1] != TypeId_Char)
				return NULL;
			return CmpStr;
		case TypeId_Enum: return CmpInt;
		case TypeId_Class: return CmpClass;
		default:
			return NULL;
	}
}

static int CmpInt(const void* a, const void* b)
{
	S64 a2 = *(S64*)&a;
	S64 b2 = *(S64*)&b;
	return a2 > b2 ? 1 : (a2 < b2 ? -1 : 0);
}

static int CmpFloat(const void* a, const void* b)
{
	double a2 = *(double*)&a;
	double b2 = *(double*)&b;
	return a2 > b2 ? 1 : (a2 < b2 ? -1 : 0);
}

static int CmpChar(const void* a, const void* b)
{
	Char a2 = *(Char*)&a;
	Char b2 = *(Char*)&b;
	return a2 > b2 ? 1 : (a2 < b2 ? -1 : 0);
}

static int CmpBit8(const void* a, const void* b)
{
	U8 a2 = *(U8*)&a;
	U8 b2 = *(U8*)&b;
	return a2 > b2 ? 1 : (a2 < b2 ? -1 : 0);
}

static int CmpBit16(const void* a, const void* b)
{
	U16 a2 = *(U16*)&a;
	U16 b2 = *(U16*)&b;
	return a2 > b2 ? 1 : (a2 < b2 ? -1 : 0);
}

static int CmpBit32(const void* a, const void* b)
{
	U32 a2 = *(U32*)&a;
	U32 b2 = *(U32*)&b;
	return a2 > b2 ? 1 : (a2 < b2 ? -1 : 0);
}

static int CmpBit64(const void* a, const void* b)
{
	U64 a2 = *(U64*)&a;
	U64 b2 = *(U64*)&b;
	return a2 > b2 ? 1 : (a2 < b2 ? -1 : 0);
}

static int CmpStr(const void* a, const void* b)
{
	const Char* a2 = (const Char*)((U8*)a + 0x10);
	const Char* b2 = (const Char*)((U8*)b + 0x10);
	return wcscmp(a2, b2);
}

static int CmpClass(const void* a, const void* b)
{
	// TODO:
}

static void* CatBin(int num, void** bins)
{
	S64 len = 0;
	int i;
	for (i = 0; i < num; i++)
		len += *(S64*)((U8*)bins[i] + 0x08);
	{
		U8* result = (U8*)AllocMem(0x18 + (size_t)len);
		((S64*)result)[0] = DefaultRefCntFunc;
		((S64*)result)[1] = len;
		((S64*)result)[2] = (S64)num;
		{
			U8* ptr = result + 0x18;
			for (i = 0; i < num; i++)
			{
				size_t size = (size_t)*(S64*)((U8*)bins[i] + 0x08);
				memcpy(ptr, (U8*)bins[i] + 0x10, size);
				ptr += size;
				ASSERT(*(S64*)bins[i] == 1);
				FreeMem(bins[i]);
			}
		}
		return result;
	}
}
