//
// Created by al32163 on 10/24/2024
//

#include "HAKCYAMLParser.h"
#include <execinfo.h>
#include <iostream>
#include "HAKCAnalysis/CommonHAKCAnalysis.h"

namespace hakc {

    HAKCYAMLParser::HAKCYAMLParser(Module &M) : M(M) {
        METHODS = new std::map<std::string, std::set<std::string>>(); 
        tmp.reserve(100); 
        ParseArchYaml();
    }

    std::map<std::string, std::set<std::string>> *HAKCYAMLParser::GetMethods(){
        // for(auto it = METHODS->cbegin(); it != METHODS->cend(); ++it)
        // {
        //     CommonHAKCAnalysis::getWriter() << "METHOD NAME: " <<  it->first << " size: " << it->second.size() << "\n";
        // }

        // CommonHAKCAnalysis::getWriter() << "getmethods size: " << (*METHODS).size() << "\n";
        return METHODS;
    }

    void HAKCYAMLParser::ParseArchYaml() {
        std::string yaml_file = HAKC_ARCH_CONFIG;
        if (!sys::fs::exists(yaml_file)) {
            CommonHAKCAnalysis::getWriter() << "Could not find YAML file " << yaml_file << "\n";
            throw std::exception();
        } else if (!sys::fs::is_regular_file(yaml_file)) {
            CommonHAKCAnalysis::getWriter() << yaml_file << " is not a regular file\n";
            throw std::exception();
        }

        YamlHAKCInformation yi;
        ErrorOr<std::unique_ptr<MemoryBuffer>> mb = MemoryBuffer::getFile(yaml_file);
        yaml::Input yin(mb.get()->getMemBufferRef().getBuffer());

        assert(!yin.error() && "Error parsing yaml file");
        // yaml is actually parsed here, for some reason
        yin >> yi;

        CommonHAKCAnalysis::getWriter() << "Arch set: " << yi.SYSTEMINFO.ARCH << "\n";
        ARCH = yi.SYSTEMINFO.ARCH;
        CommonHAKCAnalysis::getWriter() << "Platform set: " << yi.SYSTEMINFO.PLATFORM << "\n";
        PLATFORM = yi.SYSTEMINFO.PLATFORM;
        int i = 0; 
        for (YamlMethodsInformation &method : yi.SYSTEMINFO.METHODS) {
            // if method name is not in map
            // StringRef name = *&StringRef(method.NAME); 
            StringRef name = method.NAME; 
            tmp.push_back(name);
            // if (METHODS->find(name) == METHODS->end()) {
            if (METHODS->find(name.str()) == METHODS->end()) {
                CommonHAKCAnalysis::getWriter() << "orig name: " << name << ":\n";
                // memory issue was probably strange temporary conversion to stringref which did not persist 
                // (*METHODS)[tmp[i].str()] = std::set<std::string>();
                // (*METHODS)[tmp[i].str()] = std::set<StringRef>();
                (*METHODS)[tmp[i].str()] = std::set<std::string>();
            }
            for (std::string function : method.FUNCTIONS) {
                (*METHODS)[tmp[i].str()].insert(function);
                CommonHAKCAnalysis::getWriter() << "size: " << (*METHODS)[name.str()].size() << ":\n";
                CommonHAKCAnalysis::getWriter() << "\t" << function << "\n";
            }
            i++; 
        }
    }

    void HAKCYAMLParser::PrintStack() {
        // print stack debug
        void *buffer[10];
        int size = backtrace(buffer, 10);
        char **strings = backtrace_symbols(buffer, size);
        for (int i = 0; i < size; i++) {
            std::cout << strings[i] << std::endl;
        }
        free(strings);
    }

}// namespace hakc
