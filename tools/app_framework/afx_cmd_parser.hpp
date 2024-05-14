// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <algorithm>

namespace rps
{
    class ICmdArg
    {
        std::string Name;
        bool        IsRequired : 1;
        bool        IsPersistent : 1;

    public:
        template <typename T>
        T* AsPtr()
        {
            return static_cast<T*>(GetValuePointer(sizeof(T)));
        }

    protected:
        ICmdArg(const std::string& name, bool bPersistent = true, bool bRequired = false)
            : Name(name)
            , IsRequired(bRequired)
            , IsPersistent(bPersistent)
        {
        }

        virtual ~ICmdArg()
        {
        }

        virtual int32_t Parse(int32_t numRemainingArgs, char** ppRemainingArgs) = 0;

        virtual void SerializeValue(std::ostream& s)
        {
        }

        virtual void* GetValuePointer(size_t expectedSize)
        {
            return nullptr;
        }

        friend class CLI;
    };

    class CLI
    {
    public:
        static void Parse(int* pArgc, char*** pArgv)
        {
            Instance()->ParseImpl(*pArgc, *pArgv);

            *pArgc = int((Instance()->m_UnparsedArgs).size());
            *pArgv = &*Instance()->m_UnparsedArgs.begin();
        }

        static void Parse(int argc, char** argv)
        {
            Instance()->ParseImpl(argc, argv);
        }

        static void LoadConfig(const std::string& fileName)
        {
            Instance()->Load(fileName);
        }

        static void SaveConfig(const std::string& fileName)
        {
            Instance()->Save(fileName);
        }

        static ICmdArg* FindCmdArg(const std::string& name)
        {
            return Instance()->FindRegisteredCmdArg(name);
        }

    private:
        static CLI* Instance()
        {
            static CLI s_Instance;
            return &s_Instance;
        }

        bool ParseImpl(int32_t argc, char** argv)
        {
            std::unordered_set<ICmdArg*> requiredArgs;

            for (auto& registered : m_RegisteredArgs)
            {
                if (registered.second->IsRequired)
                {
                    requiredArgs.insert(registered.second);
                }
            }

            m_UnparsedArgs.push_back(argv[0]);

            for (int32_t iArg = 1; iArg < argc; iArg++)
            {
                auto iter = m_RegisteredArgs.find(argv[iArg]);
                if (iter != m_RegisteredArgs.end())
                {
                    int32_t numArgsConsumed = iter->second->Parse(argc - iArg - 1, &argv[iArg + 1]);
                    if (numArgsConsumed < 0)
                    {
                        fprintf_s(stderr, "\nError parsing command arg '%s'.", iter->second->Name.c_str());
                        return false;
                    }

                    iArg += numArgsConsumed;

                    if (iter->second->IsRequired)
                    {
                        requiredArgs.erase(iter->second);
                    }
                }
                else
                {
                    m_UnparsedArgs.push_back(argv[iArg]);
                }
            }

            if (!requiredArgs.empty())
            {
                for (auto& required : requiredArgs)
                {
                    fprintf_s(stderr, "\nRequired command arg '%s' not specified.", required->Name.c_str());
                }
                return false;
            }

            return true;
        }

        void Register(const char* prefix, const char* name, ICmdArg* pArg)
        {
            auto fullName = std::string(prefix) + name;
            auto result = m_RegisteredArgs.insert(std::make_pair(fullName, pArg));
            if (!result.second)
            {
                fprintf_s(stderr, "\nDuplicated command argument name '%s'.", fullName.c_str());
            }
        }

        void Load(const std::string& fileName)
        {
            std::ifstream fs(fileName, std::ios::in);

            if (!fs.good())
                return;

            std::string line;
            std::string name;
            std::string value;
            while (std::getline(fs, line))
            {
                if (!std::getline(fs, name, '=') || !std::getline(fs, value))
                    continue;

                auto iter = m_RegisteredArgs.find("--" + name);
                if (!value.empty() && (iter != m_RegisteredArgs.end()))
                {
                    if (!iter->second->IsPersistent)
                        continue;

                    // TODO: only support single value case atm:
                    char*   pText           = &value[0];
                    int32_t numArgsConsumed = iter->second->Parse(1, &pText);

                    if (numArgsConsumed < 1)
                    {
                        fprintf_s(stderr, "\nFailed to load argument '%s'", name.c_str());
                    }
                }
            }
        }

        void Save(const std::string& fileName)
        {
            std::ofstream fs(fileName, std::ios::out);

            if (!fs.good())
                return;

            for (auto& registered : m_RegisteredArgs)
            {
                if (registered.second->IsPersistent && (registered.first.find("--") == 0))
                {
                    fs << std::endl << registered.second->Name << "=";
                    registered.second->SerializeValue(fs);
                }
            }
        }

        ICmdArg* FindRegisteredCmdArg(const std::string& name) const
        {
            auto iter = m_RegisteredArgs.find(name);
            return (iter != m_RegisteredArgs.end()) ? iter->second : nullptr;
        }

        std::unordered_map<std::string, ICmdArg*> m_RegisteredArgs;
        std::vector<char*> m_UnparsedArgs;

        template<typename T>
        friend class CmdArg;
    };

    template<typename T>
    struct CmdArgValueParser
    {
        int32_t operator()(T* pValue, int32_t numRemainingArgs, const char* const* pStr)
        {
            return -1;
        }
    };

    template<>
    struct CmdArgValueParser<bool>
    {
        int32_t operator()(bool* pValue, int32_t numRemainingArgs, const char* const* pStr)
        {
            int consumed = 0;
            *pValue = true;

            if (numRemainingArgs > 0)
            {
                std::string trueStrs[] = { "1", "on", "true", "yes", "y" };
                std::string falseStrs[] = {"0", "off", "false", "no", "n"};

                auto trueIter = std::find_if(
                    std::begin(trueStrs), std::end(trueStrs), [=](auto i) { return i == pStr[0]; });

                if (std::end(trueStrs) != trueIter)
                {
                    consumed = 1;
                }
                auto falseIter =
                    std::find_if(std::begin(falseStrs), std::end(falseStrs), [=](auto i) { return i == pStr[0]; });

                if (std::end(falseStrs) != falseIter)
                {
                    *pValue = false;
                    consumed = 1;
                }
            }

            return consumed;
        }
    };

    template<>
    struct CmdArgValueParser<int32_t>
    {
        int32_t operator()(int32_t* pValue, int32_t numRemainingArgs, const char* const* pStr)
        {
            if (numRemainingArgs > 0)
            {
                char* pEnd = nullptr;
                int32_t parsedVal = std::strtol(pStr[0], &pEnd, 0);

                if (pEnd != pStr[0])
                {
                    *pValue = parsedVal;
                    return 1;
                }
            }

            return -1;
        }
    };

    template<>
    struct CmdArgValueParser<uint32_t>
    {
        int32_t operator()(uint32_t* pValue, int32_t numRemainingArgs, const char* const* pStr)
        {
            if (numRemainingArgs > 0)
            {
                char*    pEnd      = nullptr;
                uint32_t parsedVal = std::strtoul(pStr[0], &pEnd, 0);

                if (pEnd != pStr[0])
                {
                    *pValue = parsedVal;
                    return 1;
                }
            }

            return -1;
        }
    };

    template<>
    struct CmdArgValueParser<uint64_t>
    {
        int32_t operator()(uint64_t* pValue, int32_t numRemainingArgs, const char* const* pStr)
        {
            if (numRemainingArgs > 0)
            {
                char*    pEnd      = nullptr;
                uint64_t parsedVal = std::strtoull(pStr[0], &pEnd, 0);

                if (pEnd != pStr[0])
                {
                    *pValue = parsedVal;
                    return 1;
                }
            }

            return -1;
        }
    };

    template<>
    struct CmdArgValueParser<std::string>
    {
        int32_t operator()(std::string* pValue, int32_t numRemainingArgs, const char* const* pStr)
        {
            if (numRemainingArgs > 0)
            {
                *pValue = pStr[0];
                return 1;
            }

            return -1;
        }
    };

    template <typename T>
    class CmdArg : private ICmdArg
    {
        T Value;

    public:
        explicit CmdArg(const char*                        name,
                        const T&                           defaultVal  = {},
                        std::initializer_list<const char*> aliases     = {},
                        bool                               isPersistent = true,
                        bool                               isRequired   = false)
            : ICmdArg(name, isPersistent, isRequired)
            , Value(defaultVal)
        {
            CLI::Instance()->Register("--", name, this);
            for (auto& a : aliases)
            {
                CLI::Instance()->Register("-", a, this);
            }
        }

        virtual int32_t Parse(int32_t numRemainingArgs, char** ppRemainingArgs) override
        {
            return CmdArgValueParser<T>()(&Value, numRemainingArgs, ppRemainingArgs);
        }

        virtual void SerializeValue(std::ostream& s) override
        {
            s << Value;
        }

        virtual void* GetValuePointer(size_t expectedSize) override
        {
            return (expectedSize == sizeof(T)) ? &Value : nullptr;
        }

        operator const T& () const
        {
            return Value;
        }

        const T& get() const
        {
            return Value;
        }

        const T* operator->() const
        {
            return &Value;
        }

        const T& operator=(const T& val)
        {
            Value = val;
            return *this;
        }

        T* operator&()
        {
            return &Value;
        }

        const T* operator&() const
        {
            return &Value;
        }
    };
}
