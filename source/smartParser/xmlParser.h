#ifndef _xmlParser_h_
#define _xmlParser_h_

#include <string>
#include "tinyxml2.h"

#include "smartSequence.h"
#include "smartRecipe.h"
#include "smartVariable.h"
#include "smartStep.h"

class xmlParser
{
    public:
        xmlParser();

        bool parse(std::string fileName, smartSequence* sequence);

    private:
        bool parseVariables(tinyxml2::XMLNode* node, smartSequence* sequence);
        bool parseDynamicVariables(tinyxml2::XMLNode* node);
        bool parseRecipes(tinyxml2::XMLNode* recipes, smartSequence* sequence);
        bool parseSequence(tinyxml2::XMLNode* node);
        bool parseState(tinyxml2::XMLElement* element, smartVariable *var);
        bool parseVar(tinyxml2::XMLElement* element, smartVariable *var);
        bool parseRecipe(tinyxml2::XMLElement* recipe, smartRecipe *rec);
};
#endif

