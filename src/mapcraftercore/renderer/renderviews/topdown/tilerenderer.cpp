/*
 * Copyright 2012-2016 Moritz Hilscher
 *
 * This file is part of Mapcrafter.
 *
 * Mapcrafter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mapcrafter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mapcrafter.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tilerenderer.h"

#include "../../biomes.h"
#include "../../blockhandler.h"
#include "../../blockimages.h"
#include "../../image.h"
#include "../../overlay.h"
#include "../../rendermode.h"
#include "../../tileset.h"
#include "../../../mc/pos.h"
#include "../../../mc/worldcache.h"
#include "../../../util.h"

#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace mapcrafter {
namespace renderer {

TopdownTileRenderer::TopdownTileRenderer(const RenderView* render_view,
		BlockHandler* block_handler, BlockImages* images, int tile_width,
		mc::WorldCache* world, RenderMode* render_mode,
		std::shared_ptr<OverlayRenderMode> hardcode_overlay,
		std::vector<std::shared_ptr<OverlayRenderMode>> overlays)
	: TileRenderer(render_view, block_handler, images, tile_width, world, render_mode, hardcode_overlay, overlays) {
}

TopdownTileRenderer::~TopdownTileRenderer() {
}

namespace {

struct RenderBlock {
	mc::BlockPos pos;
	uint16_t id, data;
	bool has_full_water;
	RGBAImage block;
	std::vector<RGBAImage> block_overlays;
};

}

void TopdownTileRenderer::renderChunk(const mc::Chunk& chunk, RGBAImage& tile, std::vector<RGBAImage>& overlay_tiles, int dx, int dz) {
	int texture_size = images->getTextureSize();

	for (int x = 0; x < 16; x++) {
		for (int z = 0; z < 16; z++) {
			std::deque<RenderBlock> blocks;

			// TODO make this water thing a bit nicer
			bool in_water = false;
			bool has_full_water = false;
			int water = 0;

			mc::LocalBlockPos localpos(x, z, 0);
			//int height = chunk.getHeightAt(localpos);
			//localpos.y = height;
			localpos.y = -1;
			if (localpos.y >= mc::CHUNK_HEIGHT*16 || localpos.y < 0)
				localpos.y = mc::CHUNK_HEIGHT*16 - 1;

			uint16_t id = chunk.getBlockID(localpos);
			uint16_t data = chunk.getBlockData(localpos);
			block_handler->handleBlock(localpos.toGlobalPos(chunk.getPos()), id, data);
			while (id == 0 && localpos.y > 0) {
				localpos.y--;
				id = chunk.getBlockID(localpos);
			}
			if (localpos.y < 0)
				continue;

			while (localpos.y >= 0) {
				mc::BlockPos globalpos = localpos.toGlobalPos(chunk.getPos());

				id = chunk.getBlockID(localpos);
				data = chunk.getBlockData(localpos);
				block_handler->handleBlock(globalpos, id, data);
				if (id == 0) {
					in_water = false;
					localpos.y--;
					continue;
				}
				bool is_water = (id == 8 || id == 9) && (data & 0xf) == 0;

				if (render_mode->isHidden(globalpos, id, data)) {
					localpos.y--;
					continue;
				}

				if (is_water && !use_preblit_water) {
					has_full_water = true;
					if (is_water == in_water) {
						localpos.y--;
						continue;
					}
					in_water = is_water;
				} else if (use_preblit_water) {
					if (!is_water)
						water = 0;
					else {
						water++;
						if (water > images->getMaxWaterPreblit()) {
							auto it = blocks.begin();
							while (it != blocks.end()) {
								auto current = it++;
								if (it == blocks.end() || (it->id != 8 && it->id != 9)) {
									RenderBlock& top = *current;

									top.id = 8;
									top.data = OPAQUE_WATER;
									top.block = images->getBlock(top.id, top.data);
									drawHardcodeOverlay(top.block, top.pos, top.id, top.data);
									break;
								} else {
									blocks.erase(current);
								}
							}
							break;
						}
					}
				}

				RGBAImage block = images->getBlock(id, data);
				if (Biome::isBiomeBlock(id, data)) {
					block = images->getBiomeBlock(id, data, getBiomeOfBlock(globalpos, &chunk));
				}

				drawHardcodeOverlay(block, globalpos, id, data);

				RenderBlock render_block;
				render_block.pos = globalpos;
				render_block.id = id;
				render_block.data = data;
				render_block.has_full_water = has_full_water;
				render_block.block = block;

				for (size_t i = 0; i < overlays.size(); i++) {
					RGBAImage block_overlay = block.emptyCopy();
					overlays[i]->drawOverlay(block, block_overlay, globalpos, id, data);
					block_overlay.applyMask(block);
					render_block.block_overlays.push_back(block_overlay);
				}
				
				blocks.push_back(render_block);

				if (!images->isBlockTransparent(id, data))
					break;
				localpos.y--;
			}

			while (blocks.size() > 0) {
				RenderBlock render_block = blocks.back();
				int image_x = dx + x * texture_size;
				int image_y = dz + z * texture_size;
				tile.alphaBlit(render_block.block, image_x, image_y);
				for (size_t i = 0; i < overlays.size(); i++) {
					// lighting overlay has to render the whole rows of water blocks,
					// not just the overlay, because just overlaying water doesn't look that fancy
					if (render_block.has_full_water && overlays[i]->isBase()) {
						overlay_tiles[i].alphablit(render_block.block, image_x, image_y);
						overlay_tiles[i].alphablit(render_block.block_overlays[i], image_x, image_y, render_block.block);
					} else {
						overlay_tiles[i].blit(render_block.block_overlays[i], image_x, image_y, render_block.block);
					}
				}
				blocks.pop_back();
			}
		}
	}
}

void TopdownTileRenderer::renderTile(const TilePos& tile_pos, RGBAImage& tile, std::vector<RGBAImage>& overlay_tiles) {
	assert(overlays.size() == overlay_tiles.size());

	int texture_size = images->getTextureSize();
	tile.setSize(getTileSize(), getTileSize());
	for (size_t i = 0; i < overlays.size(); i++)
		overlay_tiles[i].setSize(getTileSize(), getTileSize());

	for (int x = 0; x < tile_width; x++) {
		for (int z = 0; z < tile_width; z++) {
			mc::ChunkPos chunkpos(tile_pos.getX() * tile_width + x, tile_pos.getY() * tile_width + z);
			current_chunk = world->getChunk(chunkpos);
			if (current_chunk != nullptr)
				renderChunk(*current_chunk, tile, overlay_tiles, texture_size*16*x, texture_size*16*z);
		}
	}
}

int TopdownTileRenderer::getTileSize() const {
	return images->getBlockSize() * 16 * tile_width;
}

}
}
