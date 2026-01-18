#pragma once
#include <vector>
#include <glm.hpp>
#include <algorithm>
#include <cmath>

struct AABB
{
    glm::vec3 c;    // center
    glm::vec3 h;    // half extents
};

static inline float clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static inline bool sphereIntersectsAABB(const glm::vec3& p, float r, const AABB& b)
{
    float cx = clampf(p.x, b.c.x - b.h.x, b.c.x + b.h.x);
    float cy = clampf(p.y, b.c.y - b.h.y, b.c.y + b.h.y);
    float cz = clampf(p.z, b.c.z - b.h.z, b.c.z + b.h.z);
    glm::vec3 q(cx, cy, cz);
    float d2 = glm::dot(p - q, p - q);
    return d2 <= r * r;
}

struct Triangle
{
    glm::vec3 a, b, c;
    AABB bounds;
};

static inline AABB triBounds(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    glm::vec3 mn = glm::min(a, glm::min(b, c));
    glm::vec3 mx = glm::max(a, glm::max(b, c));
    AABB out;
    out.c = (mn + mx) * 0.5f;
    out.h = (mx - mn) * 0.5f;
    return out;
}

// Closest point on triangle to point (Ericson)
static inline glm::vec3 closestPointOnTriangle(const glm::vec3& p,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c)
{
    const glm::vec3 ab = b - a;
    const glm::vec3 ac = c - a;
    const glm::vec3 ap = p - a;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    const glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }

    const glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

static inline bool sphereIntersectsTriangle(const glm::vec3& center, float radius,
    const Triangle& t)
{
    glm::vec3 q = closestPointOnTriangle(center, t.a, t.b, t.c);
    float d2 = glm::dot(center - q, center - q);
    return d2 <= radius * radius;
}

struct OctreeNode
{
    AABB bounds;
    std::vector<int> triIndices;
    OctreeNode* children[8]{ nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr };
    bool isLeaf = true;
};

class Octree
{
public:
    Octree() = default;
    ~Octree() { destroy(root); }

    void build(const std::vector<Triangle>& tris, int maxDepth = 8, int maxTrisPerLeaf = 30)
    {
        triangles = &tris;
        this->maxDepth = maxDepth;
        this->maxTrisPerLeaf = maxTrisPerLeaf;

        AABB b = computeAllBounds(tris);
        root = new OctreeNode();
        root->bounds = b;

        std::vector<int> idx(tris.size());
        for (int i = 0; i < (int)tris.size(); ++i) idx[i] = i;

        buildNode(root, idx, 0);
    }

    bool sphereHit(const glm::vec3& center, float radius) const
    {
        if (!root) return false;
        return sphereHitNode(root, center, radius);
    }

private:
    const std::vector<Triangle>* triangles = nullptr;
    OctreeNode* root = nullptr;
    int maxDepth = 8;
    int maxTrisPerLeaf = 30;

    static AABB computeAllBounds(const std::vector<Triangle>& tris)
    {
        glm::vec3 mn(1e9f), mx(-1e9f);
        for (auto& t : tris)
        {
            glm::vec3 tmin = t.bounds.c - t.bounds.h;
            glm::vec3 tmax = t.bounds.c + t.bounds.h;
            mn = glm::min(mn, tmin);
            mx = glm::max(mx, tmax);
        }
        AABB b;
        b.c = (mn + mx) * 0.5f;
        b.h = (mx - mn) * 0.5f;
        return b;
    }

    static bool aabbOverlap(const AABB& a, const AABB& b)
    {
        return (std::abs(a.c.x - b.c.x) <= (a.h.x + b.h.x)) &&
            (std::abs(a.c.y - b.c.y) <= (a.h.y + b.h.y)) &&
            (std::abs(a.c.z - b.c.z) <= (a.h.z + b.h.z));
    }

    void buildNode(OctreeNode* node, const std::vector<int>& idx, int depth)
    {
        node->triIndices = idx;
        node->isLeaf = true;

        if (depth >= maxDepth || (int)idx.size() <= maxTrisPerLeaf)
            return;

        node->isLeaf = false;
        glm::vec3 c = node->bounds.c;
        glm::vec3 h = node->bounds.h * 0.5f;

        for (int i = 0; i < 8; ++i)
        {
            glm::vec3 offset(
                (i & 1) ? h.x : -h.x,
                (i & 2) ? h.y : -h.y,
                (i & 4) ? h.z : -h.z
            );
            node->children[i] = new OctreeNode();
            node->children[i]->bounds.c = c + offset;
            node->children[i]->bounds.h = h;
        }

        std::vector<int> childIdx[8];
        for (int ti : idx)
        {
            const AABB& tb = (*triangles)[ti].bounds;
            for (int ci = 0; ci < 8; ++ci)
                if (aabbOverlap(tb, node->children[ci]->bounds))
                    childIdx[ci].push_back(ti);
        }

        node->triIndices.clear();

        // dacã subdivizarea explodeazã cu duplicãri -> pãstrãm leaf
        int totalChild = 0;
        for (int i = 0; i < 8; ++i) totalChild += (int)childIdx[i].size();
        if (totalChild >= (int)idx.size() * 6)
        {
            node->isLeaf = true;
            node->triIndices = idx;
            for (int i = 0; i < 8; ++i) { destroy(node->children[i]); node->children[i] = nullptr; }
            return;
        }

        for (int i = 0; i < 8; ++i)
            buildNode(node->children[i], childIdx[i], depth + 1);
    }

    bool sphereHitNode(const OctreeNode* node, const glm::vec3& center, float r) const
    {
        if (!sphereIntersectsAABB(center, r, node->bounds)) return false;

        if (node->isLeaf)
        {
            for (int ti : node->triIndices)
            {
                const Triangle& t = (*triangles)[ti];
                if (!sphereIntersectsAABB(center, r, t.bounds)) continue;
                if (sphereIntersectsTriangle(center, r, t)) return true;
            }
            return false;
        }

        for (int i = 0; i < 8; ++i)
            if (node->children[i] && sphereHitNode(node->children[i], center, r))
                return true;

        return false;
    }

    static void destroy(OctreeNode* n)
    {
        if (!n) return;
        for (int i = 0; i < 8; ++i) destroy(n->children[i]);
        delete n;
    }
};
