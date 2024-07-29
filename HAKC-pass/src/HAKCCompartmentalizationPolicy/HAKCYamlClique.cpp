//
// Created by de29664 on 7/29/24.
//

#include "HAKCCompartmentalizationPolicy/HAKCYamlClique.h"

template<>
struct yaml::ScalarEnumerationTraits<hakc::sym_color_t> {
    static void enumeration(yaml::IO &io, hakc::sym_color_t &value) {
        io.enumCase(value, "SILVER_CLIQUE", hakc::SILVER_CLIQUE);
        io.enumCase(value, "GREEN_CLIQUE", hakc::GREEN_CLIQUE);
        io.enumCase(value, "RED_CLIQUE", hakc::RED_CLIQUE);
        io.enumCase(value, "ORANGE_CLIQUE", hakc::ORANGE_CLIQUE);
        io.enumCase(value, "YELLOW_CLIQUE", hakc::YELLOW_CLIQUE);
        io.enumCase(value, "PURPLE_CLIQUE", hakc::PURPLE_CLIQUE);
        io.enumCase(value, "BLUE_CLIQUE", hakc::BLUE_CLIQUE);
        io.enumCase(value, "GREY_CLIQUE", hakc::GREY_CLIQUE);
        io.enumCase(value, "PINK_CLIQUE", hakc::PINK_CLIQUE);
        io.enumCase(value, "BROWN_CLIQUE", hakc::BROWN_CLIQUE);
        io.enumCase(value, "WHITE_CLIQUE", hakc::WHITE_CLIQUE);
        io.enumCase(value, "BLACK_CLIQUE", hakc::BLACK_CLIQUE);
        io.enumCase(value, "TEAL_CLIQUE", hakc::TEAL_CLIQUE);
        io.enumCase(value, "VIOLET_CLIQUE", hakc::VIOLET_CLIQUE);
        io.enumCase(value, "CRIMSON_CLIQUE", hakc::CRIMSON_CLIQUE);
        io.enumCase(value, "GOLD_CLIQUE", hakc::GOLD_CLIQUE);
        io.enumCase(value, "NO_CLIQUE", hakc::NO_CLIQUE);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlClique> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlClique &Clique) {
        hakc::HAKCYamlClique::YamlMapping(io, Clique);
    }
};

namespace hakc {
    HAKCYamlClique::HAKCYamlClique(sym_color_t Color, hakc_access_token_t AccessToken) : AccessToken(AccessToken),
                                                                                         Color(Color) {

    }

    void HAKCYamlClique::YamlMapping(yaml::IO &io, HAKCYamlClique &Clique) {
        io.mapRequired("access_token", Clique.AccessToken);
        io.mapRequired("name", Clique.Color);
    }
} // hakc
