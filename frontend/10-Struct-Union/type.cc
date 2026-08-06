#include "type.h"

std::shared_ptr<CType> CType::IntType = std::make_shared<CPrimaryType>(Kind::TY_Int, 4, 4);

llvm::StringRef CType::GenAnnoyRecordName(TagKind tagKind)
{
    static long long idx = 0;
    std::string name;

    if (tagKind == TagKind::kStruct)
    {
        name = "__1annoy_struct_" + std::to_string(idx++) + "_";
    }
    else
    {
        name = "__1annoy_union_" + std::to_string(idx++) + "_";
    }

    char *buf = (char *)malloc(name.size() + 1);
    memset(buf, 0, name.size());
    strcpy(buf, name.data());
    return llvm::StringRef(buf, name.size());
}

// 地址对齐算法
// 找到大于等于x,且按照align(必须是2的倍数)对其的最小地址
// (0, 1) => 0
// (1, 4) => 4
static int roundup(int x, int align)
{
    return (x + align - 1) & ~(align - 1);
}

CRecordType::CRecordType(llvm::StringRef name, const std::vector<Member> &members, TagKind tagKind) : CType(CType::TY_Record, 0, 0), name(name), members(members), tagKind(tagKind)
{
    if (tagKind == TagKind::kStruct)
    {
        UpdateStructOffset();
    }
    else
    {
        UpdateUnionOffset();
    }
}

/*
struct -> size (all element size + padding)
element -> (offset + size)
*/
void CRecordType::UpdateStructOffset()
{
    int offset = 0;
    int idx = 0;
    int totalSize = 0, maxAlign = 0, maxElementSize = 0, maxElementIdx = 0;
    for (auto &m : members)
    {
        offset = roundup(offset, m.ty->GetAlign());
        m.offset = offset;
        m.elemIdx = idx++;

        // 求出member的最大对齐数
        if (maxAlign < m.ty->GetAlign())
        {
            maxAlign = m.ty->GetAlign();
        }

        if (maxElementSize < m.ty->GetSize())
        {
            maxElementSize = m.ty->GetSize();
            maxElementIdx = m.elemIdx;
        }

        // 下一个元素的offset
        offset = offset + m.ty->GetSize();
    }

    totalSize = roundup(offset, maxAlign);

    this->size = totalSize;
    this->align = maxAlign;
    this->maxElementIdx = maxElementIdx;
}

void CRecordType::UpdateUnionOffset()
{
    int offset = 0;
    int maxAlign = 0, maxSize = 0, maxElementIdx = 0;
    int idx = 0;

    for (auto &m : members)
    {
        m.offset = 0;
        m.elemIdx = idx++;

        // 求出member的最大对齐数
        if (maxAlign < m.ty->GetAlign())
        {
            maxAlign = m.ty->GetAlign();
        }

        if (maxSize < m.ty->GetSize())
        {
            maxSize = m.ty->GetSize();
            maxElementIdx = m.elemIdx;
        }
    }

    maxSize = roundup(maxSize, maxAlign);

    this->size = maxSize;
    this->maxElementIdx = maxElementIdx;
}