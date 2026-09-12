#include "linden_common.h"
#include "llinitparam.h"
#include "../test/lltut.h"

namespace
{
    struct ScalarDefaults : LLInitParam::Block<ScalarDefaults>
    {
        Optional<S32> value;

        ScalarDefaults()
            : value("value", 7)
        {}
    };

    struct ChoiceDefaults : LLInitParam::ChoiceBlock<ChoiceDefaults>
    {
        Alternative<S32> first;
        Alternative<S32> second;

        ChoiceDefaults()
            : first("first", 11),
              second("second", 22)
        {}
    };

    struct RequiredDefaults : LLInitParam::Block<RequiredDefaults>
    {
        Mandatory<S32> value;

        RequiredDefaults()
            : value("value")
        {}
    };
}

namespace tut
{
    struct llinitparam_data {};
    typedef test_group<llinitparam_data> llinitparam_group;
    typedef llinitparam_group::object object;
    llinitparam_group llinitparamgrp("llinitparam");

    template<> template<>
    void object::test<1>()
    {
        set_test_name("scalar provided-value merge truth table");

        for (bool source_provided : {false, true})
        {
            for (bool destination_provided : {false, true})
            {
                for (bool overwrite : {false, true})
                {
                    ScalarDefaults source;
                    ScalarDefaults destination;
                    source.value.set(42, source_provided);
                    destination.value.set(19, destination_provided);

                    const bool expected_change = source_provided && (overwrite || !destination_provided);
                    const bool changed = overwrite ? destination.overwriteFrom(source) : destination.fillFrom(source);

                    ensure_equals("merge result", changed, expected_change);
                    ensure_equals("merged value", destination.value(), expected_change ? 42 : 19);
                    ensure_equals("provided state", destination.value.isProvided(), expected_change || destination_provided);
                    ensure_equals("source unchanged", source.value(), 42);
                }
            }
        }

        ScalarDefaults source;
        ScalarDefaults destination;
        source.value = 42;
        destination.value = 42;
        ensure("overwrite reports merge even for equal values", destination.overwriteFrom(source));
        ensure("base-qualified fill is a no-op", !destination.LLInitParam::BaseBlock::fillFrom(source));
    }

    template<> template<>
    void object::test<2>()
    {
        set_test_name("checkpoint choice initialization and chosen-versus-provided state");

        ChoiceDefaults first_instance;
        ensure("first construction chooses first alternative", first_instance.first.isChosen());
        ensure("initial choice is not provided", !first_instance.first.isProvided());

        ChoiceDefaults subsequent_instance;
        ensure("subsequent fresh construction has no first choice", !subsequent_instance.first.isChosen());
        ensure("subsequent fresh construction has no second choice", !subsequent_instance.second.isChosen());

        ChoiceDefaults copied(first_instance);
        ensure("copy retains choice at destination-relative offset", copied.first.isChosen());
        copied.second.choose();
        ensure("choose switches active alternative", copied.second.isChosen());
        ensure("choose alone does not mark alternative provided", !copied.second.isProvided());
        ensure("choose marks enclosing block provided", copied.isProvided());
        ensure("source choice unchanged", first_instance.first.isChosen());

        copied.second = 99;
        ensure("assignment marks alternative provided", copied.second.isProvided());
        ensure_equals("selected value", copied.second(), 99);
        copied.first.choose();
        ensure("switch clears prior provided flag", !copied.second.isProvided());
        ensure_equals("inactive alternative exposes original default", copied.second(), 22);
    }

    template<> template<>
    void object::test<3>()
    {
        set_test_name("checkpoint validation caching and provided notifications");

        RequiredDefaults params;
        ensure("missing mandatory parameter fails", !params.validateBlock(false));
        params.value = 5;
        ensure("provided mandatory parameter passes", params.validateBlock(false));
        params.value.setProvided(false);
        ensure("provided flag cleared", !params.value.isProvided());
        ensure("false notification preserves cached validation", params.validateBlock(false));
        params.paramChanged(params.value, true);
        ensure("explicit invalidation rechecks mandatory parameter", !params.validateBlock(false));
    }
}