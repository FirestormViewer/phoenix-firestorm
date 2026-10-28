/** Exercise the actual helper used in LSL include/error diagnostics. */
#include "linden_common.h"
// <FS:Beq> Extended non-ASCII path test coverage.
#include "../fslslpreprocpath.h"
#include "nonasciipaths.h"

namespace tut
{
    struct preprocpath_data {};
    using preprocpath_group = test_group<preprocpath_data>;
    using preprocpath_object = preprocpath_group::object;
    preprocpath_group preprocpath_tests("FSLSLPreprocessorPaths");

    template<> template<>
    void preprocpath_object::test<1>()
    {
        set_test_name("LSL basename with a Unicode parent and ASCII filename");
        const auto path = nonascii::name() / "include.lsl";
        ensure_equals("basename only", shortfile(nonascii::utf8(path)), std::string("include.lsl"));
    }

    template<> template<>
    void preprocpath_object::test<2>()
    {
        set_test_name("LSL Unicode script/include basename preserves UTF-8");
        const auto filename = nonascii::fs::path(u8"\u0416\u65e5\U0001f680 include.lsl");
        const auto path = nonascii::name() / "scripts" / filename;
        ensure_equals("exact UTF-8 basename", shortfile(nonascii::utf8(path)), nonascii::utf8(filename));
    }
}
// </FS:Beq>
