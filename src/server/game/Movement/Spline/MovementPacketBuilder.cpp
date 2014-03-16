/*
 * Copyright (C) 2011-2014 Project SkyFire <http://www.projectskyfire.org/>
 * Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2014 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "MovementPacketBuilder.h"
#include "MoveSpline.h"
#include "WorldPacket.h"
#include "Object.h"

namespace Movement
{
    inline void operator << (ByteBuffer& b, const Vector3& v)
    {
        b << v.x << v.y << v.z;
    }

    inline void operator >> (ByteBuffer& b, Vector3& v)
    {
        b >> v.x >> v.y >> v.z;
    }

    MonsterMoveType PacketBuilder::GetMonsterMoveType(MoveSpline const& moveSpline)
    {
        switch (moveSpline.splineflags & MoveSplineFlag::Mask_Final_Facing)
        {
            case MoveSplineFlag::Final_Target:
                return MonsterMoveFacingTarget;
            case MoveSplineFlag::Final_Angle:
                return MonsterMoveFacingAngle;
            case MoveSplineFlag::Final_Point:
                return MonsterMoveFacingPoint;
            default:
                return MonsterMoveNormal;
        }
    }

    void PacketBuilder::WriteStopMovement(Vector3 const& pos, uint32 splineId, ByteBuffer& data, Unit* unit)
    {

        ObjectGuid guid = unit->GetGUID();
        ObjectGuid transport = unit->GetTransGUID();

        data << float(0.f); // Most likely transport Y
        data << uint32(splineId);
        data << float(0.f); // Most likely transport Z
        data << float(0.f); // Most likely transport X
        data << float(pos.x);
        data << float(pos.y);
        data << float(pos.z);

        data.WriteBit(guid[3]);
        data.WriteBit(1);
        data.WriteBit(guid[6]);

        data.WriteBit(1);
        data.WriteBit(1);

        data.WriteBits(MonsterMoveStop, 3);
        data.WriteBit(1);
        data.WriteBit(guid[2]);
        data.WriteBit(guid[7]);
        data.WriteBit(guid[5]);
        data.WriteBit(1);
        data.WriteBit(guid[4]);

        data.WriteBits(0, 22); // WP count
        data.WriteBit(1);
        data.WriteBit(0);

        data.WriteBit(guid[0]);
        data.WriteBit(transport[3]);
        data.WriteBit(transport[6]);
        data.WriteBit(transport[5]);
        data.WriteBit(transport[0]);
        data.WriteBit(transport[1]);
        data.WriteBit(transport[2]);
        data.WriteBit(transport[4]);
        data.WriteBit(transport[7]);

        data.WriteBit(1);
        data.WriteBit(1); // Parabolic speed // esi+4Ch
        data.WriteBit(1);

        data.WriteBits(0, 20);

        data.WriteBit(guid[1]);
        data.WriteBit(0);
        data.WriteBit(0);
        data.WriteBit(1);

        data.FlushBits();

        data.WriteByteSeq(guid[3]);
        data.WriteByteSeq(transport[7]);
        data.WriteByteSeq(transport[3]);
        data.WriteByteSeq(transport[2]);
        data.WriteByteSeq(transport[0]);
        data.WriteByteSeq(transport[6]);
        data.WriteByteSeq(transport[4]);
        data.WriteByteSeq(transport[5]);
        data.WriteByteSeq(transport[1]);
        data.WriteByteSeq(guid[7]);
        data.WriteByteSeq(guid[5]);
        data.WriteByteSeq(guid[1]);
        data.WriteByteSeq(guid[2]);
        data.WriteByteSeq(guid[6]);
        data.WriteByteSeq(guid[0]);
        data.WriteByteSeq(guid[4]);
    }

    void WriteLinearPath(Spline<int32> const& spline, ByteBuffer& data)
    {
        uint32 last_idx = spline.getPointCount() - 3;
        Vector3 const* real_path = &spline.getPoint(1);

        if (last_idx > 0)
        {
            Vector3 middle = (real_path[0] + real_path[last_idx]) / 2.f;
            Vector3 offset;
            // first and last points already appended
            for (uint32 i = 0; i < last_idx; ++i)
            {
                offset = middle - real_path[i];
                data.appendPackXYZ(offset.x, offset.y, offset.z);
            }
        }
    }

    void WriteUncompressedPath(Spline<int32> const& spline, ByteBuffer& data)
    {
        data.append<Vector3>(&spline.getPoint(2), spline.getPointCount() - 3);
    }

    void WriteUncompressedCyclicPath(Spline<int32> const& spline, ByteBuffer& data)
    {
        data << spline.getPoint(1); // Fake point, client will erase it from the spline after first cycle done
        data.append<Vector3>(&spline.getPoint(1), spline.getPointCount() - 3);
    }

    void PacketBuilder::WriteMonsterMove(const MoveSpline& moveSpline, WorldPacket& data, Unit* unit)
    {
        ObjectGuid guid = unit->GetGUID();
        ObjectGuid transport = unit->GetTransGUID();
        MonsterMoveType type = GetMonsterMoveType(moveSpline);

        data << float(0.f); // Most likely transport Y
        data << uint32(moveSpline.GetId());
        data << float(0.f); // Most likely transport Z
        data << float(0.f); // Most likely transport X
        data << moveSpline.spline.getPoint(moveSpline.spline.first());
        
        data.WriteBit(guid[3]);
        data.WriteBit(!moveSpline.splineflags.raw());
        data.WriteBit(guid[6]);

        data.WriteBit(1);
        data.WriteBit(1);

        data.WriteBits(type, 3);
        data.WriteBit(1);
        data.WriteBit(guid[2]);
        data.WriteBit(guid[7]);
        data.WriteBit(guid[5]);

        if (type == MonsterMoveFacingTarget)
        {
            ObjectGuid targetGuid = moveSpline.facing.target;
            data.WriteBit(targetGuid[6]);
            data.WriteBit(targetGuid[7]);
            data.WriteBit(targetGuid[0]);
            data.WriteBit(targetGuid[5]);
            data.WriteBit(targetGuid[2]);
            data.WriteBit(targetGuid[3]);
            data.WriteBit(targetGuid[4]);
            data.WriteBit(targetGuid[1]);
        }

        data.WriteBit(1);
        data.WriteBit(guid[4]);
        
        int32 compressedSplineCount = moveSpline.splineflags & MoveSplineFlag::UncompressedPath ? 0 : moveSpline.spline.getPointCount() - 3;
        data.WriteBits(compressedSplineCount, 22); // WP count
        data.WriteBit(1);
        data.WriteBit(0);

        data.WriteBit(guid[0]);
        data.WriteBit(transport[3]);
        data.WriteBit(transport[6]);
        data.WriteBit(transport[5]);
        data.WriteBit(transport[0]);
        data.WriteBit(transport[1]);
        data.WriteBit(transport[2]);
        data.WriteBit(transport[4]);
        data.WriteBit(transport[7]);

        data.WriteBit(1);
        data.WriteBit(1); // Parabolic speed // esi+4Ch
        data.WriteBit(1);

        uint32 uncompressedSplineCount = moveSpline.splineflags & MoveSplineFlag::UncompressedPath ? moveSpline.splineflags.cyclic ? moveSpline.spline.getPointCount() - 2 : moveSpline.spline.getPointCount() - 3 : 1;
        data.WriteBits(uncompressedSplineCount,  20);

        data.WriteBit(guid[1]);
        data.WriteBit(0); // Send no block
        data.WriteBit(0);
        data.WriteBit(!moveSpline.Duration());

        data.FlushBits();

        if (type == MonsterMoveFacingTarget)
        {
            ObjectGuid targetGuid = moveSpline.facing.target;
            data.WriteByteSeq(targetGuid[5]);
            data.WriteByteSeq(targetGuid[3]);
            data.WriteByteSeq(targetGuid[6]);
            data.WriteByteSeq(targetGuid[1]);
            data.WriteByteSeq(targetGuid[4]);
            data.WriteByteSeq(targetGuid[2]);
            data.WriteByteSeq(targetGuid[0]);
            data.WriteByteSeq(targetGuid[7]);
        }

        data.WriteByteSeq(guid[3]);
        data.WriteByteSeq(transport[7]);
        data.WriteByteSeq(transport[3]);
        data.WriteByteSeq(transport[2]);
        data.WriteByteSeq(transport[0]);
        data.WriteByteSeq(transport[6]);
        data.WriteByteSeq(transport[4]);
        data.WriteByteSeq(transport[5]);
        data.WriteByteSeq(transport[1]);

        if (type == MonsterMoveFacingAngle)
            data << float(moveSpline.facing.angle);

        if (moveSpline.splineflags.raw())
            data << uint32(moveSpline.splineflags.raw());

        data.WriteByteSeq(guid[7]);

        if (compressedSplineCount)
            WriteLinearPath(moveSpline.spline, data);

        data.WriteByteSeq(guid[5]);
        data.WriteByteSeq(guid[1]);
        data.WriteByteSeq(guid[2]);

        if (moveSpline.splineflags & MoveSplineFlag::UncompressedPath)
        {
            if (moveSpline.splineflags.cyclic)
                WriteUncompressedCyclicPath(moveSpline.spline, data);
            else
                WriteUncompressedPath(moveSpline.spline, data);
        }
        else
            data << moveSpline.spline.getPoint(moveSpline.spline.getPointCount() - 2);

        data.WriteByteSeq(guid[6]);

        if (moveSpline.Duration())
            data << uint32(moveSpline.Duration());

        if (type == MonsterMoveFacingPoint)
            data << moveSpline.facing.f.x << moveSpline.facing.f.y << moveSpline.facing.f.z;

        data.WriteByteSeq(guid[0]);
        data.WriteByteSeq(guid[4]);

    }

    void PacketBuilder::WriteCreateBits(MoveSpline const& moveSpline, ByteBuffer& data)
    {
        if (!data.WriteBit(!moveSpline.Finalized()))
            return;

        if (!data.WriteBit(!moveSpline.Finalized()))
            return;

        data.WriteBit(moveSpline.splineflags & (MoveSplineFlag::Parabolic | MoveSplineFlag::Animation));
        data.WriteBits(moveSpline.spline.mode(), 2);
        data.WriteBits(moveSpline.getPath().size(), 20);
        data.WriteBits(moveSpline.splineflags.raw(), 25);
        data.WriteBit((moveSpline.splineflags & MoveSplineFlag::Parabolic) && moveSpline.effect_start_time < moveSpline.Duration());

        bool hasUnkPart = false;
        
        data.WriteBit(hasUnkPart);

        if (hasUnkPart)
        {
            data.WriteBits(0, 2);                            //unk word300
            data.WriteBits(0, 21);                           //unk dword284
        }
    }

    void PacketBuilder::WriteCreateData(MoveSpline const& moveSpline, ByteBuffer& data, bool* hasTarget, ByteBuffer* targetGUID)
    {
        *hasTarget = false;                                 // set it to false by default

        if (!moveSpline.Finalized())
        {
            MoveSplineFlag const& splineFlags = moveSpline.splineflags;
            MonsterMoveType type = GetMonsterMoveType(moveSpline);

            data << float(1.f);                             // splineInfo.duration_mod_next; added in 3.1

            uint32 nodes = moveSpline.getPath().size();
            for (uint32 i = 0; i < nodes; ++i)
            {
                data << float(moveSpline.getPath()[i].z);
                data << float(moveSpline.getPath()[i].y);
                data << float(moveSpline.getPath()[i].x);
            }

            data << uint8(type);

            data << float(1.f);                             // splineInfo.duration_mod; added in 3.1

            //unk part goes here

            if(type == MonsterMoveFacingPoint)
                data << moveSpline.facing.f.x << moveSpline.facing.f.y << moveSpline.facing.f.z;

            if ((splineFlags & MoveSplineFlag::Parabolic) && moveSpline.effect_start_time < moveSpline.Duration())
                data << moveSpline.vertical_acceleration;   // added in 3.1

            if (type = MonsterMoveFacingAngle)
                data << moveSpline.facing.angle;

            data << moveSpline.Duration();

            if (splineFlags & (MoveSplineFlag::Parabolic | MoveSplineFlag::Animation))
                data << moveSpline.effect_start_time;       // added in 3.1

            data << moveSpline.timePassed();
        }

        data << moveSpline.GetId();

        Vector3 destination = moveSpline.isCyclic() ? Vector3::zero() : moveSpline.FinalDestination();
        data << destination;
    }

    void PacketBuilder::WriteFacingTargetPart(MoveSpline const& moveSpline, ByteBuffer& data)
    {
        if (GetMonsterMoveType(moveSpline) == MonsterMoveFacingTarget && !moveSpline.Finalized())
        {
            ObjectGuid facingGuid = moveSpline.facing.target;
            data.WriteBit(facingGuid[6]);
            data.WriteBit(facingGuid[7]);
            data.WriteBit(facingGuid[3]);
            data.WriteBit(facingGuid[0]);
            data.WriteBit(facingGuid[5]);
            data.WriteBit(facingGuid[1]);
            data.WriteBit(facingGuid[4]);
            data.WriteBit(facingGuid[2]);
            data.WriteByteSeq(facingGuid[4]);
            data.WriteByteSeq(facingGuid[2]);
            data.WriteByteSeq(facingGuid[5]);
            data.WriteByteSeq(facingGuid[6]);
            data.WriteByteSeq(facingGuid[0]);
            data.WriteByteSeq(facingGuid[7]);
            data.WriteByteSeq(facingGuid[1]);
            data.WriteByteSeq(facingGuid[3]);
        }
    }
}
