#include "XmlReader.hpp"
#include "tinyxml2.hpp"

#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {
XmlReader::XmlReader(std::string filePath) {
    doc_ = std::make_shared<XMLDocument>();
    if(!filePath.empty()) {
        throw invalid_value_exception("XmlReader::XmlReader: filePath is empty!");
    }

    if(!utils::fileExists(filePath.c_str())) {
        throw invalid_value_exception("XmlReader::XmlReader: file not exist!");
    }

    auto rc = doc_->LoadFile(filePath.c_str());
    if(rc != 0) {
        throw invalid_value_exception(utils::to_string() << "XmlReader::XmlReader: load file failed!, rc=" << rc);
    }

    rootXMLElement_ = doc_->RootElement();
    if(!rootXMLElement_) {
        throw invalid_value_exception("XmlReader::XmlReader: root element is null!");
    }
}

XmlReader::XmlReader(const char *buffer, size_t size) {
    doc_ = std::make_shared<XMLDocument>();

    if(!buffer || size == 0) {
        throw invalid_value_exception("XmlReader::XmlReader: buffer is null or size is 0!");
    }

    auto rc = doc_->Parse(buffer, size);
    if(rc != 0) {
        throw invalid_value_exception(utils::to_string() << "XmlReader::XmlReader: parse buffer failed!, rc=" << rc);
    }

    rootXMLElement_ = doc_->RootElement();
    if(!rootXMLElement_) {
        throw invalid_value_exception("XmlReader::XmlReader: root element is null!");
    }
}

XMLElement *XmlReader::getRootElement() const {
    return rootXMLElement_;
}

bool XmlReader::getTextOfLeafNode(const std::string &nodePathName, std::string &text) {
    std::string errorMessage;
    if(nodePathName.empty()) {
        return false;
    }
    auto NodeList        = utils::split(nodePathName, ".");
    auto rootXMLElement  = rootXMLElement_;
    auto ChildrenElement = rootXMLElement_;
    for(size_t i = 0; i < NodeList.size(); ++i) {
        libobsensor::XMLElement *childrenElement = nullptr;
        childrenElement                          = rootXMLElement->FirstChildElement(NodeList[i].c_str());
        if(!childrenElement) {
            return false;
        }
        if(i < NodeList.size() - 1) {
            rootXMLElement = childrenElement;
        }
        if(i == NodeList.size() - 1) {
            ChildrenElement = childrenElement;
        }
    }

    if(ChildrenElement == rootXMLElement_) {
        return false;
    }

    if(!ChildrenElement->FirstChild() || !ChildrenElement->FirstChild()->ToText()) {
        return false;
    }
    text = ChildrenElement->GetText();
    return true;
}

bool XmlReader::isNodeContained(const std::string &nodePathName) {
    std::string errorMessage;
    if(nodePathName.empty()) {
        return false;
    }
    auto NodeList = utils::split(nodePathName, ".");
    if(NodeList.empty()) {
        return false;
    }
    auto   rootXMLElement  = rootXMLElement_;
    auto   ChildrenElement = rootXMLElement_;
    size_t count           = 0;
    for(size_t i = 0; i < NodeList.size(); ++i) {
        XMLElement *childrenElement = nullptr;
        childrenElement             = rootXMLElement->FirstChildElement(NodeList[i].c_str());
        if(!childrenElement) {
            return false;
        }
        count++;
        if(count != NodeList.size()) {
            rootXMLElement = childrenElement;
        }
        if(count == NodeList.size()) {
            ChildrenElement = childrenElement;
        }
    }
    return true;
}

bool XmlReader::getBooleanValue(const std::string &nodePathName, bool &t) {
    std::string text;
    bool        status = getTextOfLeafNode(nodePathName, text);
    if(!status) {
        return false;
    }
    status = utils::string2Boolean(text, t);
    if(!status) {
        return false;
    }
    return true;
}

bool XmlReader::getIntValue(const std::string &nodePathName, int &t) {
    std::string text;
    bool        status = getTextOfLeafNode(nodePathName, text);
    if(!status) {
        return false;
    }
    status = utils::string2Int(text, t);
    if(!status) {
        return false;
    }
    return true;
}

bool XmlReader::getFloatValue(const std::string &nodePathName, float &t) {
    std::string text;
    bool        status = getTextOfLeafNode(nodePathName, text);
    if(!status) {
        return false;
    }
    status = utils::string2Float(text, t);
    if(!status) {
        return false;
    }
    return true;
}

bool XmlReader::getDoubleValue(const std::string &nodePathName, double &t) {
    std::string text;
    bool        status = getTextOfLeafNode(nodePathName, text);
    if(!status) {
        return false;
    }
    status = utils::string2Double(text, t);
    if(!status) {
        return false;
    }
    return true;
}

bool XmlReader::getStringValue(const std::string &nodePathName, std::string &t) {
    std::string text;
    bool        status = getTextOfLeafNode(nodePathName, text);
    if(!status) {
        return false;
    }
    text = utils::clearHeadAndTailSpace(text);
    if(text.empty()) {
        return false;
    }
    t = text;
    return true;
}

}  // namespace libobsensor