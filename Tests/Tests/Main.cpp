#include <cstdio>

#include <Arch/x86/Cpuid.h>
#include <Arch/x86/Hyper-V.h>
#include <Arch/x86/Interrupts.h>
#include <Arch/x86/Intrinsics.h>
#include <Arch/x86/Msr.h>
#include <Arch/x86/Pte.h>
#include <Arch/x86/Registers.h>
#include <Arch/x86/Segmentation.h>
#include <Arch/x86/Svm.h>
#include <Arch/x86/Vmx.h>

void cpuid()
{
    const auto basicInfo = Cpuid::Cpuid::query<Cpuid::Generic::MaximumFunctionNumberAndVendorId>();
    union
    {
        char id[sizeof(unsigned int) * 3 + sizeof('\0')];
        struct
        {
            unsigned int part0;
            unsigned int part1;
            unsigned int part2;
        } raw;
    } id{};
    id.raw.part0 = basicInfo->VendorPart1;
    id.raw.part1 = basicInfo->VendorPart2;
    id.raw.part2 = basicInfo->VendorPart3;
    printf("ID: %s\n", id.id);

    const auto brand0 = Cpuid::Cpuid::query<Cpuid::Generic::ProcessorBrandString0>();
    const auto brand1 = Cpuid::Cpuid::query<Cpuid::Generic::ProcessorBrandString1>();
    const auto brand2 = Cpuid::Cpuid::query<Cpuid::Generic::ProcessorBrandString2>();

    union
    {
        char brandString[sizeof(Cpuid::RawCpuid) * 3 + sizeof('\0')];
        struct
        {
            Cpuid::RawCpuid part0;
            Cpuid::RawCpuid part1;
            Cpuid::RawCpuid part2;
        } raw;
    } brandString{};
    brandString.raw.part0.regs = brand0.raw.regs;
    brandString.raw.part1.regs = brand1.raw.regs;
    brandString.raw.part2.regs = brand2.raw.regs;

    printf("CPU: %s\n", brandString.brandString);

    if (basicInfo->isIntel())
    {
        const auto features = Cpuid::Cpuid::query<Cpuid::Intel::FeatureInformation>();
        printf(
            "  SSE    : %u\n"
            "  SSE2   : %u\n"
            "  SSE3   : %u\n"
            "  SSSE3  : %u\n"
            "  SSE4.1 : %u\n"
            "  SSE4.2 : %u",
            features->SSE,
            features->SSE2,
            features->SSE3,
            features->SSSE3,
            features->SSE41,
            features->SSE42
        );
    }
    else if (basicInfo->isAmd())
    {
        const auto features = Cpuid::Cpuid::query<Cpuid::Amd::FeatureInformation>();
        printf(
            "  SSE    : %u\n"
            "  SSE2   : %u\n"
            "  SSE3   : %u\n"
            "  SSSE3  : %u\n"
            "  SSE4.1 : %u\n"
            "  SSE4.2 : %u",
            features->SSE,
            features->SSE2,
            features->SSE3,
            features->SSSE3,
            features->SSE41,
            features->SSE42
        );
    }
    else
    {
        printf("Unknown vendor!\n");
    }

}

template <typename Type>
typename Type::Layout* phys2virt(const Type /*phys*/)
{
    return nullptr;
}

#ifdef _WIN64
void paging()
{
    constexpr auto k_mode = Pte::Mode::longMode4Level;

    using LinearAddress = Pte::LinearAddress<k_mode>;
    using Tables = Pte::Tables<k_mode>;

    const auto cpuid = Cpuid::Cpuid::query<Cpuid::Intel::FeatureInformation>();
    cpuid.layout.ACPI;

    const LinearAddress addr{ .raw = 0x00007FFF'F8000000 };
    const auto cr3Pfn = Regs::Native::Cr3::query()->paging4Level.PML4;

    auto cr4 = Regs::Native::Cr4::query();
    cr4.write();
    

    const auto* const pml4e = phys2virt(Tables::pml4e(cr3Pfn, addr));
    const auto* const pdpe = phys2virt(pml4e->pdpe(addr));
    switch (pdpe->pageSize())
    {
    case Pte::PageSize::nonPse:
    {
        const auto* const pde = phys2virt(pdpe->nonPse.pde(addr));
        switch (pde->pageSize())
        {
        case Pte::PageSize::nonPse:
        {
            // 4Kb:
            const auto* const pte = phys2virt(pde->nonPse.pte(addr));
            const auto phys = pte->physicalAddress(addr);
            break;
        }
        case Pte::PageSize::pse:
        {
            // 2Mb:
            const auto phys = pde->pse.physicalAddress(addr);
            break;
        }
        }
        break;
    }
    case Pte::PageSize::pse:
    {
        // 1Gb:
        const auto phys = pdpe->pse.physicalAddress(addr);
        break;
    }
    }
}
#endif

int main()
{
    cpuid();
    return 0;
}
