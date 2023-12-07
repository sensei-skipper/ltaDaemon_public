#include <iostream>
#include "xmlParser.h"

using namespace std;

xmlParser::xmlParser()
{
}

bool xmlParser::parse(string fileName, smartSequence* sequence)
{
    bool ret = false;
    tinyxml2::XMLDocument xml_doc;
    tinyxml2::XMLNode *node;

    // Load XML file.
    tinyxml2::XMLError eResult = xml_doc.LoadFile(fileName.c_str());
    if (eResult != tinyxml2::XML_SUCCESS) return false;

    // <variables></variables>
    node = xml_doc.FirstChildElement("variables");
    if (node == nullptr)
    {
        return false;
    }
    else
    {
        if(!parseVariables(node, sequence)) return false;
    }

    // <dynamicVariables></dynamicVariables>
    node = xml_doc.FirstChildElement("dynamicVariables");
    if (node == nullptr)
    {
        return false;
    }
    else
    {
        if(!parseDynamicVariables(node)) return false;
    }

    // <recipes></recipes>
    node = xml_doc.FirstChildElement("recipes");
    if (node == nullptr)
    {
        return false;
    }
    else
    {
        if(!parseRecipes(node, sequence)) return false;
    }

    // <sequence></sequence>
    node = xml_doc.FirstChildElement("sequence");
    if (node == nullptr)
    {
        return false;
    }
    else
    {
        if(!parseSequence(node)) return false;
    }

    return true;
}

bool xmlParser::parseVariables(tinyxml2::XMLNode* node, smartSequence* sequence)
{
    tinyxml2::XMLElement* element;

    // <state></state>
    element = node->FirstChildElement("state");
    while (element != nullptr)
    {
        smartVariable var;
        parseState(element, &var);
        element = element->NextSiblingElement("state");
        sequence->addVariable(var.name(), var);
    }

    // <var></var>
    element = node->FirstChildElement("var");
    while (element != nullptr)
    {
        smartVariable var;
        parseVar(element, &var);
        sequence->addVariable(var.name(), var);
        element = element->NextSiblingElement("var");
    }

    return true;
}

bool xmlParser::parseDynamicVariables(tinyxml2::XMLNode* node)
{

    return true;
}

bool xmlParser::parseRecipes(tinyxml2::XMLNode* recipes, smartSequence* sequence)
{
    tinyxml2::XMLElement* element;

    // <recipe></recipe>
    element = recipes->FirstChildElement("recipe");
    while (element != nullptr)
    {
        smartRecipe recipe;
        parseRecipe(element, &recipe);
        sequence->addRecipe(recipe.name(), recipe);
        element = element->NextSiblingElement("recipe");
    }

    return true;
}

bool xmlParser::parseSequence(tinyxml2::XMLNode* node)
{

    return true;
}

bool xmlParser::parseState(tinyxml2::XMLElement* element, smartVariable *var)
{
    const char *attText = nullptr;

    // Get attributes.
    attText = element->Attribute("name");
    if ( attText == nullptr) return false;
    string name = attText;

    attText = element->Attribute("val");
    if ( attText == nullptr) return false;
    string val = attText;

    // Set variable attributes.
    var->set("state", name, val);

    return true;
}

bool xmlParser::parseVar(tinyxml2::XMLElement* element, smartVariable *var)
{
    const char *attText = nullptr;

    // Get attributes.
    attText = element->Attribute("name");
    if ( attText == nullptr) return false;
    string name = attText;

    attText = element->Attribute("val");
    if ( attText == nullptr) return false;
    string val = attText;

    // Set variable attributes.	
    var->set("var", name, val);

    return true;
}

bool xmlParser::parseRecipe(tinyxml2::XMLElement* recipe, smartRecipe *rec)
{
    const char *attText = nullptr;
    tinyxml2::XMLElement* element;

    // Get recipe name.	
    attText = recipe->Attribute("name");
    if ( attText == nullptr) return false;
    string name = attText;

    rec->setName(name);

    // <step></step>
    element = recipe->FirstChildElement("step");
    while (element != nullptr)
    {
        smartStep step;	
        attText = element->Attribute("state");
        if ( attText == nullptr) return false;
        string state = attText;

        attText = element->Attribute("delay");
        if ( attText == nullptr) return false;
        string delay = attText;

        // Set step attributes.
        step.set(state, delay);

        // Add step into recipe.
        rec->addStep(step);

        element = element->NextSiblingElement("step");
    }

    return true;
}

