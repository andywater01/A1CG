/*
NOU Framework - Created for INFR 2310 at Ontario Tech.
(c) Samantha Stahlke 2020

Texture.h
Simple wrapper for STB image loading into 2D textures.
*/

#pragma once

#include "glad/glad.h"

#include <string>

namespace nou
{
	class 2D
	{
		public:

		Image2D(const std::string& filename, bool useNearest = false);
		~Texture2D();

		GLuint GetID() const;
		void GetDimensions(int& width, int& height) const;

		private:

		GLuint m_id;
		int m_width, m_height;
	};
}