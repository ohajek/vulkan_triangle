#pragma once

#include <vector>
#include <array>
#include <glm/glm.hpp>

struct Vertex {
	glm::vec2 position;
	glm::vec3 color;

	/*
	Returns description of at which rate to load data from memory throughout the vertices
	*/
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; //Move to next data after each VERTEX, can be INSTANCE

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};
		/*
		Format types:	
			float: VK_FORMAT_R32_SFLOAT
			vec2: VK_FORMAT_R32G32_SFLOAT
			vec3: VK_FORMAT_R32G32B32_SFLOAT
			vec4: VK_FORMAT_R32G32B32A32_SFLOAT

			ivec2: VK_FORMAT_R32G32_SINT, a 2-component vector of 32-bit signed integers
			uvec4: VK_FORMAT_R32G32B32A32_UINT, a 4-component vector of 32-bit unsigned integers
			double: VK_FORMAT_R64_SFLOAT, a double-precision (64-bit) float
		*/
		//Position attribute description
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		//Color attribute description
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		return attributeDescriptions;
	}
};