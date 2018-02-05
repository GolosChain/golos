#pragma once

#include <fc/string.hpp>
#include <fc/time.hpp>

namespace golos {
    namespace type_traits {
        template<bool>
        struct static_range;
    }

    namespace protocol {

        /*
         * This class represents the basic versioning scheme of the Golos blockchain.
         * All versions are a triple consisting of a major version, hardfork version, and release version.
         * It allows easy comparison between versions. A version is a read only object.
         */
        struct version {
            version() {
            }

            version(uint8_t m, uint8_t h, uint16_t r);

            uint8_t major() const;

            uint8_t hardfork() const;

            uint16_t release() const;

            virtual ~version() {
            }

            bool operator==(const version &o) const {
                return v_num == o.v_num;
            }

            bool operator!=(const version &o) const {
                return v_num != o.v_num;
            }

            bool operator<(const version &o) const {
                return v_num < o.v_num;
            }

            bool operator<=(const version &o) const {
                return v_num <= o.v_num;
            }

            bool operator>(const version &o) const {
                return v_num > o.v_num;
            }

            bool operator>=(const version &o) const {
                return v_num >= o.v_num;
            }

            operator fc::string() const;

            uint32_t v_num = 0;
        };

        struct hardfork_version : version {
            hardfork_version() : version() {
            }

            hardfork_version(uint8_t m, uint8_t h) : version(m, h, 0) {
            }

            hardfork_version(version v) {
                v_num = v.v_num & 0xFFFF0000;
            }

            ~hardfork_version() {
            }

            void operator=(const version &o) {
                v_num = o.v_num & 0xFFFF0000;
            }

            void operator=(const hardfork_version &o) {
                v_num = o.v_num & 0xFFFF0000;
            }

            bool operator==(const hardfork_version &o) const {
                return v_num == o.v_num;
            }

            bool operator!=(const hardfork_version &o) const {
                return v_num != o.v_num;
            }

            bool operator<(const hardfork_version &o) const {
                return v_num < o.v_num;
            }

            bool operator<=(const hardfork_version &o) const {
                return v_num <= o.v_num;
            }

            bool operator>(const hardfork_version &o) const {
                return v_num > o.v_num;
            }

            bool operator>=(const hardfork_version &o) const {
                return v_num >= o.v_num;
            }

            bool operator==(const version &o) const {
                return v_num == (o.v_num & 0xFFFF0000);
            }

            bool operator!=(const version &o) const {
                return v_num != (o.v_num & 0xFFFF0000);
            }

            bool operator<(const version &o) const {
                return v_num < (o.v_num & 0xFFFF0000);
            }

            bool operator<=(const version &o) const {
                return v_num <= (o.v_num & 0xFFFF0000);
            }

            bool operator>(const version &o) const {
                return v_num > (o.v_num & 0xFFFF0000);
            }

            bool operator>=(const version &o) const {
                return v_num >= (o.v_num & 0xFFFF0000);
            }
        };

        struct hardfork_version_vote {
            hardfork_version_vote() {
            }

            hardfork_version_vote(const hardfork_version &v, fc::time_point_sec t) : hf_version(v), hf_time(t) {
            }

            hardfork_version hf_version;
            fc::time_point_sec hf_time;
        };

        template<uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class static_version {
        public:
            static const version version_instance;
        };

        template<template<uint8_t Major, uint8_t Hardfork, uint16_t Release, typename ...>
                class Versionable, uint8_t Major, uint8_t Hardfork, uint16_t Release>
        class versionable_to {
        public:
            typedef Versionable<Major, Hardfork, Release> versioned_type;

            virtual versioned_type version_to() const = 0;
        };

        template<uint8_t MinorVersion, uint8_t HardforkVersion, uint16_t ReleaseVersion> const version static_version<
                MinorVersion, HardforkVersion, ReleaseVersion>::version_instance = version(MinorVersion,
                                                                                           HardforkVersion,
                                                                                           ReleaseVersion);
    }
} // golos::protocol

namespace fc {
    class variant;

    void to_variant(const golos::protocol::version &v, variant &var);

    void from_variant(const variant &var, golos::protocol::version &v);

    void to_variant(const golos::protocol::hardfork_version &hv, variant &var);

    void from_variant(const variant &var, golos::protocol::hardfork_version &hv);
} // fc

#include <fc/reflect/reflect.hpp>

FC_REFLECT((golos::protocol::version), (v_num))
FC_REFLECT_DERIVED((golos::protocol::hardfork_version), ((golos::protocol::version)),)

FC_REFLECT((golos::protocol::hardfork_version_vote), (hf_version)(hf_time))