//TODO ?? can we preinstantiate each type and write as .hpp and .cpp
#pragma once
#include "resource_holder.hpp"
#include <string>
#include <SFML/Graphics/Font.hpp>
#include <iostream>
#include <sstream>

template<typename Identifier, typename Resource>
void ResourceHolder<Identifier, Resource>::Load(const Identifier id, const std::string& filename)
{
    bool loaded = false;
    std::unique_ptr<Resource> resource(new Resource());
    if constexpr (std::is_same_v<Resource, sf::Font>)
    {
        loaded = resource->openFromFile(filename);
    }
    else
    {
        loaded = resource->loadFromFile(filename);
    }
    if (!loaded)
    {
        std::ostringstream error_msg;
        error_msg << "ResourceHolder::Load failed to load file: " << filename 
                  << " (ResourceID: " << static_cast<int>(id) << ")";
        throw std::runtime_error(error_msg.str());
    }
    auto inserted = m_resource_map.insert(std::make_pair(id, std::move(resource)));
    if (!inserted.second)
    {
        std::ostringstream error_msg;
        error_msg << "ResourceHolder::Load failed: duplicate resource ID detected (ID: " 
                  << static_cast<int>(id) << "). Resource was already loaded.";
        throw std::runtime_error(error_msg.str());
    }
}

template<typename Identifier, typename Resource>
template<typename Parameter>
void ResourceHolder<Identifier, Resource>::Load(const Identifier id, const std::string& filename, const Parameter& second_param)
{
    std::unique_ptr<Resource> resource(new Resource());
    if (!resource->loadFromFile(filename, second_param))
    {
        std::ostringstream error_msg;
        error_msg << "ResourceHolder::Load failed to load file: " << filename 
                  << " with additional parameter (ResourceID: " << static_cast<int>(id) << ")";
        throw std::runtime_error(error_msg.str());
    }
    auto inserted = m_resource_map.insert(std::make_pair(id, std::move(resource)));
    if (!inserted.second)
    {
        std::ostringstream error_msg;
        error_msg << "ResourceHolder::Load failed: duplicate resource ID detected (ID: " 
                  << static_cast<int>(id) << "). Resource was already loaded.";
        throw std::runtime_error(error_msg.str());
    }
}

template<typename Identifier, typename Resource>
const Resource& ResourceHolder<Identifier, Resource>::Get(Identifier id) const
{
    auto found = m_resource_map.find(id);
    if (found == m_resource_map.end())
    {
        std::ostringstream error_msg;
        error_msg << "ResourceHolder::Get failed: Resource not found (ResourceID: " 
                  << static_cast<int>(id) << "). Total resources in map: " 
                  << m_resource_map.size();
        std::cerr << "[ResourceHolder] " << error_msg.str() << std::endl;
        throw std::runtime_error(error_msg.str());
    }
    return *found->second;
}


template<typename Identifier, typename Resource>
Resource& ResourceHolder<Identifier, Resource>::Get(Identifier id)
{
    auto found = m_resource_map.find(id);
    if (found == m_resource_map.end())
    {
        std::ostringstream error_msg;
        error_msg << "ResourceHolder::Get failed: Resource not found (ResourceID: " 
                  << static_cast<int>(id) << "). Total resources in map: " 
                  << m_resource_map.size();
        std::cerr << "[ResourceHolder] " << error_msg.str() << std::endl;
        throw std::runtime_error(error_msg.str());
    }
    return *found->second;
}