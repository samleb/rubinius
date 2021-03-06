#ifndef RBX_CONFIG_PARSER
#define RBX_CONFIG_PARSER

#include <iostream>
#include <sstream>
#include <map>
#include <vector>

namespace rubinius {
  class ConfigParser {
  public:
    class Entry {
    public:
      std::string variable;
      std::string value;

      bool is_number();
      bool is_true();
      bool in_section(std::string prefix);
      long to_i();
    };

    typedef std::map<std::string, Entry*> ConfigMap;
    typedef std::vector<Entry*> EntryList;

    ConfigMap variables;

    virtual ~ConfigParser();

    void   process_argv(int argc, char** argv);
    Entry* parse_line(const char* line);
    void   import_line(const char* line);
    void   import_stream(std::istream&);
    Entry* find(std::string variable);
    EntryList* get_section(std::string prefix);
  };
}

#endif
