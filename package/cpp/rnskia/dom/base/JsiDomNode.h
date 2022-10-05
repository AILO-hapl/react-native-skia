#pragma once

#include "JsiHostObject.h"
#include "NodePropsContainer.h"
#include "NodeProp.h"

#include <vector>

#include "RNSkPlatformContext.h"

namespace RNSkia {

template <class TNode>
class JsiDomNodeCtor {
public:
  /**
   Constructor to add to the Api object
   */
  static const jsi::HostFunctionType
  createCtor(std::shared_ptr <RNSkPlatformContext> context) {
    return JSI_HOST_FUNCTION_LAMBDA{
      auto node = std::make_shared<TNode>(context);
      node->initializeNode(runtime, thisValue, arguments, count);
      return jsi::Object::createFromHostObject(runtime, std::move(node));
    };
  }
};

/**
 Implements an abstract base class for nodes in the Skia Reconciler. This node coresponds to the native implementation
 of the Node.ts class in Javascript.
 */
class JsiDomNode :
public JsiHostObject,
public std::enable_shared_from_this<JsiDomNode> {
public:
  /**
   Contructor. Takes as parameters the values comming from the JS world that initialized the class.
   */
  JsiDomNode(std::shared_ptr<RNSkPlatformContext> context, const char* type) :
    _type(type),
    _context(context),
    JsiHostObject() {}
  
  /**
   Called when creating the node, resolves properties from the node constructor. These
   properties are materialized, ie. no animated values or anything.
   */
  JSI_HOST_FUNCTION(initializeNode) {
    return setProps(runtime, thisValue, arguments, count);
  }
  
  /**
   JS-function for setting the properties from the JS reconciler on the node.
   */
  JSI_HOST_FUNCTION(setProps) {
    setProps(runtime, getArgumentAsObject(runtime, arguments, count, 0));
    return jsi::Value::undefined();
  }
  
  /**
   Empty setProp implementation - compatibility with JS node
   */
  JSI_HOST_FUNCTION(setProp) {
    return jsi::Value::undefined();
  }
  
  /**
   JS Function to be called when the node is no longer part of the reconciler tree. Use for cleaning up.
   */
  JSI_HOST_FUNCTION(dispose) {
    dispose();
    return jsi::Value::undefined();
  }
  
  /**
   JS Function for adding a child node to this node.
   */
  JSI_HOST_FUNCTION(addChild) {
    // child: Node<unknown>
    auto newChild = getArgumentAsHostObject<JsiDomNode>(runtime, arguments, count, 0);
    addChild(newChild);
    return jsi::Value::undefined();
  }
  
  /*
   JS Function for removing a child node from this node
   */
  JSI_HOST_FUNCTION(removeChild) {
    auto child = getArgumentAsHostObject<JsiDomNode>(runtime, arguments, count, 0);
    removeChild(child);
    return jsi::Value::undefined();
  }
  
  /**
   JS Function for insering a child node to a specific location in the children array on this node
   */
  JSI_HOST_FUNCTION(insertChildBefore) {
    // child: Node<unknown>, before: Node<unknown>
    auto child = getArgumentAsHostObject<JsiDomNode>(runtime, arguments, count, 0);
    auto before = getArgumentAsHostObject<JsiDomNode>(runtime, arguments, count, 1);
    insertChildBefore(child, before);
    return jsi::Value::undefined();
  }
  
  /**
   JS Function for getting child nodes for this node
   */
  JSI_HOST_FUNCTION(children) {
    auto array = jsi::Array(runtime, _children.size());
    
    size_t index = 0;
    for (auto child: _children) {
      array.setValueAtIndex(runtime, index++, child->asHostObject(runtime));
    }
    return array;
  }
  
  /**
   JS Property for getting the type of node
   */
  JSI_PROPERTY_GET(type) {
    return jsi::String::createFromUtf8(runtime, getType());
  }
  
  JSI_EXPORT_PROPERTY_GETTERS(JSI_EXPORT_PROP_GET(JsiDomNode, type))
  
  JSI_EXPORT_FUNCTIONS(JSI_EXPORT_FUNC(JsiDomNode, setProps),
                       JSI_EXPORT_FUNC(JsiDomNode, setProp),
                       JSI_EXPORT_FUNC(JsiDomNode, addChild),
                       JSI_EXPORT_FUNC(JsiDomNode, removeChild),
                       JSI_EXPORT_FUNC(JsiDomNode, insertChildBefore),
                       JSI_EXPORT_FUNC(JsiDomNode, children),
                       JSI_EXPORT_FUNC(JsiDomNode, dispose))
  
  /**
   Returns the node type.
  */
  const char *getType() { return _type; };
  
  /**
   Returns the container for node properties
   */
  NodePropsContainer* getPropsContainer() {
    return _propsContainer.get();
  }
  
  /**
   Callback that will be called when the node is disposed - typically registered from the dependency
   manager so that nodes can be removed and unsubscribed from when removed from the reconciler tree.
   */
  void setDisposeCallback(std::function<void()> disposeCallback) {
    _disposeCallback = disposeCallback;
  }
  
protected:
  /**
   Override to define properties in node implementations
   */
  virtual void defineProperties(NodePropsContainer* container) {};
  
  /**
   Returns the platform context
   */
  std::shared_ptr<RNSkPlatformContext> getContext() { return _context; }
  
  /**
   Returns this node as a host object that can be returned to the JS side.
  */
  jsi::Object asHostObject(jsi::Runtime &runtime) {
    return jsi::Object::createFromHostObject(runtime, shared_from_this());
  }
  
  /**
   Native implementation of the set properties method. This is called from the reconciler when
   properties are set due to changes in React. This method will always call the onPropsSet method
   as a signal that things have changed.
   */
  void setProps(jsi::Runtime &runtime, jsi::Object &&props) {
    if (_propsContainer == nullptr) {
      
      // Initialize properties container
      _propsContainer = std::make_shared<NodePropsContainer>();
      
      // Ask sub classes to define their properties
      defineProperties(_propsContainer.get());      
      
    }
    // Update properties container
    _propsContainer->setProps(runtime, std::move(props));
  };
  
  /**
   Returns all child JsiDomNodes for this node.
   */
  const std::vector<std::shared_ptr<JsiDomNode>> &getChildren() {
    return _children;
  }
  
  /**
   Adds a child node to the array of children for this node
   */
  virtual void addChild(std::shared_ptr<JsiDomNode> child) {
    _children.push_back(child);
  }
  
  /**
   Inserts a child node before a given child node in the children array for this node
   */
  virtual void
  insertChildBefore(std::shared_ptr<JsiDomNode> child, std::shared_ptr<JsiDomNode> before) {
    auto position = std::find(_children.begin(), _children.end(), before);
    _children.insert(position, child);
  }
  
  /**
   Removes a child. Removing a child will remove the child from the array of children and call dispose on the child node.
   */
  virtual void removeChild(std::shared_ptr<JsiDomNode> child) {
    _children.erase(std::remove_if(_children.begin(), _children.end(),
                                   [child](const auto &node) { return node == child; }),
                    _children.end());
    
    // We don't need to call dispose since the dtor handles disposing
    child->dispose();
  }
  
  /**
   Clean up resources in use by the node. We have to explicitly call dispose when the node is removed from the
   reconciler tree, since due to garbage collection we can't be sure that the destructor is called when the node is
   removed - JS might hold a reference that will later be GC'ed.
   */
  void dispose() {
    if (_disposeCallback != nullptr) {
      _disposeCallback();
      _disposeCallback = nullptr;
    }
  }
  
private:
  std::shared_ptr<RNSkPlatformContext> _context;
  std::vector<std::shared_ptr<JsiDomNode>> _children;
  std::shared_ptr<NodePropsContainer> _propsContainer;
  const char* _type;
  std::function<void()> _disposeCallback;
};

}
