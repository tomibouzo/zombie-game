#include "Gameplay/Procedural/HouseOfficePlan.h"
#include "Gameplay/Procedural/HouseSizing.h"
#include "Gameplay/Procedural/HouseNavigation.h"

namespace
{
bool IsService(EHouseRoomType Type) { return Type == EHouseRoomType::Bathroom || Type == EHouseRoomType::Storage; }
bool Adjacent(const FHouseGenerationSettings& S, const FHouseRoom& A, const FHouseRoom& B)
{
	int32 Length = 0;
	if (A.Max.X == B.Min.X || B.Max.X == A.Min.X) Length = FMath::Min(A.Max.Y,B.Max.Y)-FMath::Max(A.Min.Y,B.Min.Y);
	if (A.Max.Y == B.Min.Y || B.Max.Y == A.Min.Y) Length = FMath::Max(Length,FMath::Min(A.Max.X,B.Max.X)-FMath::Max(A.Min.X,B.Min.X));
	return Length*S.CellSize >= S.DoorWidth+2*S.WallThickness;
}
}

bool FHouseOfficePlan::Build(const FHouseGenerationSettings& S, FHouseLayout& Out, TArray<FIntPoint>& Connections, FString& Error)
{
	Out = FHouseLayout(); Connections.Reset();
	FRandomStream Random(S.Seed);
	for (int32 Attempt=0; Attempt<64; ++Attempt)
	{
		FHouseLayout L; L.Rooms.SetNum(S.RoomCount);
		L.FootprintCells = FIntPoint(S.WidthCells,S.DepthCells);
		L.Rooms[0].RoomType = EHouseRoomType::Reception;
		for (int32 I=1; I<S.RoomCount; ++I)
		{
			L.Rooms[I].RoomType = I==1 ? EHouseRoomType::Bathroom : I==2 ? EHouseRoomType::Storage : EHouseRoomType::Office;
			L.Rooms[I].Purpose = I==1 ? EHouseRoomPurpose::Default : I==2 ? EHouseRoomPurpose::Storage : EHouseRoomPurpose::Office;
		}
		TArray<int32> Order{0};
		for (int32 I=1; I<S.RoomCount; ++I) Order.Add(I);
		for (int32 I=Order.Num()-1; I>1; --I) Order.Swap(I,Random.RandRange(1,I));
		int32 Budget = 1024;
		TFunction<bool(int32,int32,FIntPoint,FIntPoint)> Pack = [&](int32 Start,int32 Count,FIntPoint Min,FIntPoint Max)
		{
			if (--Budget<0) return false;
			if (Count==1)
			{
				auto& R=L.Rooms[Order[Start]];
				if (!FHouseSizing::Fits(S,R,Max-Min) || FMath::Min(Max.X-Min.X,Max.Y-Min.Y)*S.CellSize < S.DoorWidth+2*S.WallThickness) return false;
				R.Min=Min; R.Max=Max; return true;
			}
			struct FCut { int32 Count, Axis, Position; };
			TArray<FCut> Cuts;
			for (int32 N=1; N<Count; ++N)
				for (int32 Axis=0; Axis<2; ++Axis)
					for (int32 Cut=Min[Axis]+2; Cut<=Max[Axis]-2; ++Cut)
					{
						FIntPoint SplitMax=Max, SplitMin=Min; SplitMax[Axis]=Cut; SplitMin[Axis]=Cut;
						auto Possible = [&](int32 Offset,int32 Num,FIntPoint Size)
						{
							double Minimum=0,Maximum=0;
							for (int32 I=Offset; I<Offset+Num; ++I)
							{
								const auto& Range=FHouseSizing::Range(S,L.Rooms[Order[I]],false);
								Minimum+=Range.MinArea; Maximum+=Range.MaxArea;
							}
							const double Area=Size.X*Size.Y*FMath::Square(S.CellSize/100.0);
							// Gross area includes internal walls. Conservative upper allowance avoids pruning valid partitions.
							return Area>=Minimum && Area<=Maximum+Num*2*(Size.X+Size.Y)*S.CellSize*S.WallThickness/10000.0;
						};
						if (Possible(Start,N,SplitMax-Min) && Possible(Start+N,Count-N,Max-SplitMin)) Cuts.Add({N,Axis,Cut});
					}
			for (int32 I=Cuts.Num()-1; I>0; --I) Cuts.Swap(I,Random.RandRange(0,I));
			for (const auto& Cut : Cuts)
			{
				if (Budget<=0) break;
				FIntPoint SplitMax=Max,SplitMin=Min; SplitMax[Cut.Axis]=Cut.Position; SplitMin[Cut.Axis]=Cut.Position;
				if (Pack(Start,Cut.Count,Min,SplitMax) && Pack(Start+Cut.Count,Count-Cut.Count,SplitMin,Max)) return true;
			}
			return false;
		};
		if (!Pack(0,S.RoomCount,FIntPoint::ZeroValue,L.FootprintCells)) continue;
		TArray<FIntPoint> Edges;
		TSet<int32> Reached{0};
		while (Reached.Num()<S.RoomCount-2)
		{
			TArray<FIntPoint> Frontier;
			for (int32 A=0; A<S.RoomCount; ++A)
				for (int32 B=A+1; B<S.RoomCount; ++B)
					if (!IsService(L.Rooms[A].RoomType) && !IsService(L.Rooms[B].RoomType) && Reached.Contains(A)!=Reached.Contains(B) && Adjacent(S,L.Rooms[A],L.Rooms[B])) Frontier.Add(FIntPoint(A,B));
			if (Frontier.IsEmpty()) break;
			const auto Edge=Frontier[Random.RandRange(0,Frontier.Num()-1)]; Edges.Add(Edge); Reached.Add(Edge.X); Reached.Add(Edge.Y);
		}
		if (Reached.Num()!=S.RoomCount-2) continue;
		for (int32 Service : {1,2})
		{
			TArray<int32> Neighbors;
			for (int32 I=0; I<S.RoomCount; ++I) if (Reached.Contains(I) && Adjacent(S,L.Rooms[Service],L.Rooms[I])) Neighbors.Add(I);
			if (Neighbors.IsEmpty()) break;
			Edges.Add(FIntPoint(Service,Neighbors[Random.RandRange(0,Neighbors.Num()-1)]));
		}
		if (Edges.Num()!=S.RoomCount-1) continue;
		for (int32 A=0; A<S.RoomCount; ++A)
			for (int32 B=A+1; B<S.RoomCount; ++B)
				if (!IsService(L.Rooms[A].RoomType) && !IsService(L.Rooms[B].RoomType) && !Edges.Contains(FIntPoint(A,B)) &&
					Adjacent(S,L.Rooms[A],L.Rooms[B]) && Random.FRand()<S.ExtraConnectionChance) Edges.Add(FIntPoint(A,B));
		Connections=MoveTemp(Edges); Out=MoveTemp(L); return true;
	}
	Error=TEXT("Office suite could not fit its room sizes and connected workrooms in 64 bounded packing attempts. Adjust footprint/count or Reception (Living Room), Office, Storage and Bathroom size limits.");
	return false;
}

bool FHouseOfficePlan::Validate(const FHouseLayout& L,FString& Error)
{
	int32 Reception=0, Offices=0, Storage=0, Bathroom=0;
	TArray<int32> Degree; Degree.Init(0,L.Rooms.Num());
	for (const auto& W : L.Walls)
		if (W.Opening==EHouseOpening::Door && W.RoomB!=INDEX_NONE)
		{
			if (!L.Rooms.IsValidIndex(W.RoomA) || !L.Rooms.IsValidIndex(W.RoomB)) return false;
			++Degree[W.RoomA]; ++Degree[W.RoomB];
			if (IsService(L.Rooms[W.RoomA].RoomType) && IsService(L.Rooms[W.RoomB].RoomType)) { Error=TEXT("Office service rooms cannot connect to each other."); return false; }
		}
	for (int32 I=0; I<L.Rooms.Num(); ++I)
	{
		const auto& R=L.Rooms[I];
		const auto Purpose = R.RoomType==EHouseRoomType::Office ? EHouseRoomPurpose::Office : R.RoomType==EHouseRoomType::Storage ? EHouseRoomPurpose::Storage : EHouseRoomPurpose::Default;
		if (R.Purpose!=Purpose) { Error=TEXT("Office room purpose does not match its type."); return false; }
		Reception+=R.RoomType==EHouseRoomType::Reception; Offices+=R.RoomType==EHouseRoomType::Office;
		Storage+=R.RoomType==EHouseRoomType::Storage; Bathroom+=R.RoomType==EHouseRoomType::Bathroom;
		if (IsService(R.RoomType) && Degree[I]!=1) { Error=TEXT("Office bathroom and storage must each have one workroom/reception door."); return false; }
	}
	const auto Report=FHouseNavigation::Measure(L);
	if (Reception!=1 || Storage!=1 || Bathroom!=1 || Offices!=L.Rooms.Num()-3 || Report.EntranceRoom==INDEX_NONE ||
		L.Rooms[Report.EntranceRoom].RoomType!=EHouseRoomType::Reception || Report.RoomDoorDepths.Contains(INDEX_NONE))
	{ Error=TEXT("Office program needs one entrance reception, one bathroom, one storage room and connected offices."); return false; }
	return true;
}

FHouseGenerationSettings FHouseOfficePlan::Preset(int32 Seed)
{
	FHouseGenerationSettings S;
	S.Seed=Seed; S.Navigation.Pattern=EHouseLayoutPattern::ConnectedOffice;
	S.WidthCells=10; S.DepthCells=8; S.RoomCount=6; S.bAllowLargerFootprint=false;
	S.ExtraConnectionChance=0.5f; S.FurnitureArrangementAttempts=4;
	FHouseRoomRule Desk;
	Desk.RuleName=TEXT("Office desks"); Desk.MatchTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Room.Type.Office")));
	Desk.RequiredFurniture.Add(EHouseFurnitureType::Desk); Desk.bRequireMatchingRoom=true;
	S.RoomRules.Add(Desk);
	return S;
}
