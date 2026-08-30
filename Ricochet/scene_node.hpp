#pragma once
#include <SFML/Graphics.hpp>
#include "receiver_categories.hpp"
#include "command.hpp"
#include "command_queue.hpp"

#include <set>


class SceneNode : public sf::Transformable, public sf::Drawable
{
public:
	typedef std::unique_ptr<SceneNode> Ptr;
	typedef std::pair<SceneNode*, SceneNode*> Pair;

public:
	SceneNode(ReceiverCategories cateogry = ReceiverCategories::kNone);
	void AttachChild(Ptr child);
	Ptr DetachChild(const SceneNode& node);

	void Update(sf::Time dt, CommandQueue& commands);

	sf::Vector2f GetWorldPosition() const;
	sf::Transform GetWorldTransform() const;

	void OnCommand(const Command& command, sf::Time dt);

	virtual sf::FloatRect GetBoundingRect() const;
	void DrawBoundingRect(sf::RenderTarget& target, sf::RenderStates states, sf::FloatRect& rect) const;
	void CheckSceneCollision(SceneNode& scene_graph, std::set<Pair>& collision_pairs);
	virtual unsigned int GetCategory() const;
	void RemoveWrecks();

protected:
	virtual void UpdateCurrent(sf::Time dt, CommandQueue& commands);

private:
	void UpdateChildren(sf::Time dt, CommandQueue& commands);

	virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
	virtual void DrawCurrent(sf::RenderTarget& target, sf::RenderStates states) const;
	void DrawChildren(sf::RenderTarget& target, sf::RenderStates states) const;


	void CheckNodeCollision(SceneNode& node, std::set<Pair>& collision_pairs);
	virtual bool IsMarkedForRemoval() const;
	virtual bool IsDestroyed() const;


private:
	std::vector<Ptr> m_children;
	SceneNode* m_parent;
	ReceiverCategories m_default_category;
};
float Distance(const SceneNode& lhs, const SceneNode& rhs);
bool Collision(const SceneNode& lhs, const SceneNode& rhs);

