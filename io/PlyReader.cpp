/******************************************************************************
* Copyright (c) 2015, Peter J. Gadomski <pete.gadomski@gmail.com>
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following
* conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in
*       the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of Hobu, Inc. or Flaxen Geo Consulting nor the
*       names of its contributors may be used to endorse or promote
*       products derived from this software without specific prior
*       written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
* OF SUCH DAMAGE.
****************************************************************************/

#include "PlyReader.hpp"

#include <sstream>

#include <pdal/PDALUtils.hpp>
#include <pdal/PointView.hpp>
#include <pdal/pdal_macros.hpp>
#include <pdal/util/IStream.hpp>

namespace pdal
{

std::string PlyReader::readLine()
{
    m_line.clear();
    if (m_lines.size())
    {
        m_line = m_lines.top();
        m_lines.pop();
    }
    else
    {
        do
        {
            std::getline(*m_stream, m_line);
        } while (m_line.empty() && m_stream->good());
    }
    Utils::trimTrailing(m_line);
    m_linePos = Utils::extract(m_line, 0,
        [](char c){ return !std::isspace(c); });
    return std::string(m_line, 0, m_linePos);
}


void PlyReader::pushLine()
{
    m_lines.push(m_line);
}


std::string PlyReader::nextWord()
{
    std::string s;
    std::string::size_type cnt =
        Utils::extract(m_line, m_linePos, (int(*)(int))std::isspace);
    m_linePos += cnt;
    if (m_linePos == m_line.size())
        return s;

    cnt = Utils::extract(m_line, m_linePos,
        [](char c){ return !std::isspace(c); });
    s = std::string(m_line, m_linePos, cnt);
    m_linePos += cnt;
    return s;
}


void PlyReader::extractMagic()
{
    std::string first = readLine();
    if (first != "ply")
        throwError("File isn't a PLY file.  'ply' not found.");
    if (m_linePos != m_line.size())
        throwError("Text found following 'ply' keyword.");
}


void PlyReader::extractEnd()
{
    std::string first = readLine();
    if (first != "end_header")
        throwError("'end_header' expected but found line beginning with '" +
            first + "' instead.");
    if (m_linePos != m_line.size())
        throwError("Text found following 'end_header' keyword.");
}


void PlyReader::extractFormat()
{
    std::string word = readLine();
    if (word != "format")
        throwError("Expected format line not found in PLY file.");

    word = nextWord();
    if (word == "ascii")
        m_format = Format::ASCII;
    else if (word == "binary_big_endian")
        m_format = Format::BINARY_BE;
    else if (word == "binary_little_endian")
        m_format = Format::BINARY_LE;
    else
        throwError("Unrecognized PLY format: '" + word + "'.");

    word = nextWord();
    if (word != "1.0")
        throwError("Unsupported PLY version: '" + word + "'.");
}

Dimension::Type PlyReader::getType(const std::string& name)
{
    static std::map<std::string, Dimension::Type> types =
    {
        { "int8", Dimension::Type::Signed8 },
        { "uint8", Dimension::Type::Unsigned8 },
        { "int16", Dimension::Type::Signed16 },
        { "uint16", Dimension::Type::Unsigned16 },
        { "int32", Dimension::Type::Signed32 },
        { "uint32", Dimension::Type::Unsigned32 },
        { "float32", Dimension::Type::Float },
        { "float64", Dimension::Type::Double },

        { "char", Dimension::Type::Signed8 },
        { "uchar", Dimension::Type::Unsigned8 },
        { "short", Dimension::Type::Signed16 },
        { "ushort", Dimension::Type::Unsigned16 },
        { "int", Dimension::Type::Signed32 },
        { "uint", Dimension::Type::Unsigned32 },
        { "float", Dimension::Type::Float },
        { "double", Dimension::Type::Double }
    };

    try
    {
        return types.at(name);
    }
    catch (std::out_of_range)
    {}
    return Dimension::Type::None;
}


void PlyReader::extractProperty(Element& element)
{
    std::string word = nextWord();
    Dimension::Type type = getType(word);

    if (type != Dimension::Type::None)
    {
        std::string name = nextWord();
        if (name.empty())
            throwError("No name for property of element '" +
                element.m_name + "'.");
        element.m_properties.push_back(
            std::unique_ptr<Property>(new SimpleProperty(name, type)));
    }
    else if (word == "list")
    {
        if (element.m_name == "vertex")
            throwError("List properties are not supported for the 'vertex' "
                "element.");

        word = nextWord();
        Dimension::Type countType = getType(word);
        if (countType == Dimension::Type::None)
            throwError("No valid count type for list property of element '" +
                element.m_name + "'.");
        word = nextWord();
        Dimension::Type listType = getType(word);
        if (listType == Dimension::Type::None)
            throwError("No valid list type for list property of element '" +
                element.m_name + "'.");
        std::string name = nextWord();
        if (name.empty())
            throwError("No name for property of element '" +
                element.m_name + "'.");
        element.m_properties.push_back(
            std::unique_ptr<Property>(new ListProperty(name, countType,
                listType)));
    }
    else
        throwError("Invalid property type '" + word + "'.");
}


void PlyReader::extractProperties(Element& element)
{
    while (true)
    {
        std::string word = readLine();
        if (word == "comment" || word == "obj_info")
            continue;
        else if (word == "property")
            extractProperty(element);
        else
        {
            pushLine();
            break;
        }
    }
}


bool PlyReader::extractElement()
{
    std::string word = readLine();
    if (word == "comment" || word == "obj_info")
        return true;
    else if (word == "end_header")
    {
        pushLine();
        return false;
    }
    else if (word == "element")
    {
        std::string name = nextWord();
        if (name.empty())
            throwError("Missing element name.");
        long count = std::stol(nextWord());
        if (count < 0)
            throwError("Invalid count for element '" + name + "'.");
        m_elements.emplace_back(name, count);
        extractProperties(m_elements.back());
        return true;
    }
    throwError("Invalid keyword '" + word + "' when expecting an element.");
    return false;  // quiet compiler
}


void PlyReader::extractHeader()
{
    extractMagic();
    extractFormat();
    while (extractElement())
        ;
    extractEnd();
    m_dataPos = m_stream->tellg();
}


static PluginInfo const s_info = PluginInfo(
        "readers.ply",
        "Read ply files.",
        "http://pdal.io/stages/reader.ply.html");


CREATE_STATIC_PLUGIN(1, 0, PlyReader, Reader, s_info);


std::string PlyReader::getName() const
{
    return s_info.name;
}


PlyReader::PlyReader()
{}


void PlyReader::initialize()
{
    m_stream = Utils::openFile(m_filename, true);
    extractHeader();
    Utils::closeFile(m_stream);
    m_stream = nullptr;
}


void PlyReader::addDimensions(PointLayoutPtr layout)
{
    SimpleProperty *prop;

    for (auto& elt : m_elements)
    {
        if (elt.m_name == "vertex")
        {
            for (auto& prop : elt.m_properties)
            {
                auto vprop = static_cast<SimpleProperty *>(prop.get());
                layout->registerOrAssignDim(vprop->m_name, vprop->m_type);
                vprop->setDim(
                    layout->registerOrAssignDim(vprop->m_name, vprop->m_type));
            }
            return;
        }
    }
    throwError("No 'vertex' element in header.");
}


bool PlyReader::readProperty(Property *prop, PointRef& point)
{
    if (!m_stream->good())
        return false;
    prop->read(m_stream, m_format, point);
    return true;
}


void PlyReader::SimpleProperty::read(std::istream *stream,
    PlyReader::Format format, PointRef& point)
{
    if (format == Format::ASCII)
    {
        double d;
        *stream >> d;
        point.setField(m_dim, d);
    }
    else if (format == Format::BINARY_LE)
    {
        ILeStream in(stream);
        Everything e = Utils::extractDim(in, m_type);
        point.setField(m_dim, m_type, &e);
    }
    else if (format == Format::BINARY_BE)
    {
        IBeStream in(stream);
        Everything e = Utils::extractDim(in, m_type);
        point.setField(m_dim, m_type, &e);
    }
}


// Right now we don't support list properties for point data.  We just
// read the data and throw it away.
void PlyReader::ListProperty::read(std::istream *stream,
    PlyReader::Format format, PointRef& point)
{
    if (format == Format::ASCII)
    {
        size_t cnt;
        *stream >> cnt;

        double d;
        while (cnt--)
            *stream >> d;
    }
    else if (format == Format::BINARY_LE)
    {
        ILeStream istream(stream);
        Everything e = Utils::extractDim(istream, m_countType);
        size_t cnt = (size_t)Utils::toDouble(e, m_countType);
        cnt *= Dimension::size(m_listType);
        istream.seek(cnt, std::ios_base::cur);
    }
    else if (format == Format::BINARY_BE)
    {
        IBeStream istream(stream);
        Everything e = Utils::extractDim(istream, m_countType);
        size_t cnt = (size_t)Utils::toDouble(e, m_countType);
        cnt *= Dimension::size(m_listType);
        istream.seek(cnt, std::ios_base::cur);
    }
}


void PlyReader::readElement(Element& elt, PointRef& point)
{
    for (auto& prop : elt.m_properties)
        if (!readProperty(prop.get(), point))
            throwError("Error reading data for point/element " +
                std::to_string(point.pointId()) + ".");
}


void PlyReader::ready(PointTableRef table)
{
    m_stream = Utils::openFile(m_filename, true);
    if (m_stream)
        m_stream->seekg(m_dataPos);
}


point_count_t PlyReader::read(PointViewPtr view, point_count_t num)
{
    point_count_t cnt;

    PointRef point(view->point(0));
    for (Element& elt : m_elements)
    {
        cnt = 0;
        for (PointId idx = 0; idx < elt.m_count && idx < num; ++idx)
        {
            point.setPointId(idx);
            readElement(elt, point);
            cnt++;
        }
        // We currently only load the vertex data.
        if (elt.m_name == "vertex")
            break;
    }
    return cnt;
}


void PlyReader::done(PointTableRef table)
{
    Utils::closeFile(m_stream);
}

} // namespace pdal
