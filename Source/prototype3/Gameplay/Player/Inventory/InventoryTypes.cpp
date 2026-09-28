#include "Gameplay/Player/Inventory/InventoryTypes.h"
#include "Gameplay/Items/ItemDefinition.h"

namespace
{
double Cross(FVector2D A, FVector2D B) { return A.X * B.Y - A.Y * B.X; }
double SignedArea(const TArray<FVector2D>& Points)
{
	double Area = 0;
	for (int32 I = 0; I < Points.Num(); ++I) Area += Cross(Points[I], Points[(I + 1) % Points.Num()]);
	return Area * 0.5;
}
bool HasSeparatingAxis(const FInventoryShapePart& Axes, const FInventoryShapePart& Other)
{
	for (int32 I = 0; I < Axes.Vertices.Num(); ++I)
	{
		const FVector2D Edge = Axes.Vertices[(I + 1) % Axes.Vertices.Num()] - Axes.Vertices[I];
		const FVector2D Axis = FVector2D(-Edge.Y, Edge.X) / Edge.Size();
		double MinA = TNumericLimits<double>::Max(), MaxA = -MinA;
		double MinB = MinA, MaxB = MaxA;
		for (FVector2D P : Axes.Vertices) { const double D = FVector2D::DotProduct(P, Axis); MinA = FMath::Min(MinA, D); MaxA = FMath::Max(MaxA, D); }
		for (FVector2D P : Other.Vertices) { const double D = FVector2D::DotProduct(P, Axis); MinB = FMath::Min(MinB, D); MaxB = FMath::Max(MaxB, D); }
		if (MaxA <= MinB + InventoryGeometry::Tolerance || MaxB <= MinA + InventoryGeometry::Tolerance) return true;
	}
	return false;
}
}

bool InventoryGeometry::IsFinite(FVector2D Point) { return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y); }
double InventoryGeometry::NormalizeAngle(double Degrees)
{
	double Angle = FMath::Fmod(Degrees, 360.0);
	if (Angle < 0) Angle += 360;
	return Angle == 0 ? 0 : Angle;
}

bool FInventoryShapePart::IsValid() const
{
	if (Vertices.Num() < 3 || Vertices.Num() > 256) return false;
	for (FVector2D P : Vertices)
		if (!InventoryGeometry::IsFinite(P) || FMath::Abs(P.X) > 4096 || FMath::Abs(P.Y) > 4096) return false;
	const double Area = SignedArea(Vertices);
	if (FMath::Abs(Area) <= InventoryGeometry::Tolerance) return false;
	const double Sign = Area > 0 ? 1 : -1;
	for (int32 I = 0; I < Vertices.Num(); ++I)
	{
		const FVector2D Edge = Vertices[(I + 1) % Vertices.Num()] - Vertices[I];
		if (Edge.SizeSquared() <= InventoryGeometry::Tolerance) return false;
		for (int32 J = 0; J < Vertices.Num(); ++J)
		{
			if (J != I && Vertices[J].Equals(Vertices[I], InventoryGeometry::Tolerance)) return false;
			// Every point must be inside every edge: also rejects self-intersections.
			if (Sign * Cross(Edge, Vertices[J] - Vertices[I]) < -InventoryGeometry::Tolerance) return false;
		}
	}
	return true;
}

bool FInventoryShapePart::Contains(FVector2D Point) const
{
	if (Vertices.Num() < 3 || !InventoryGeometry::IsFinite(Point)) return false;
	const double Sign = SignedArea(Vertices) > 0 ? 1 : -1;
	for (int32 I = 0; I < Vertices.Num(); ++I)
		if (Sign * Cross(Vertices[(I + 1) % Vertices.Num()] - Vertices[I], Point - Vertices[I]) < -InventoryGeometry::Tolerance) return false;
	return true;
}

bool FInventoryShapePart::Overlaps(const FInventoryShapePart& Other) const
{
	return Vertices.Num() >= 3 && Other.Vertices.Num() >= 3
		&& !HasSeparatingAxis(*this, Other) && !HasSeparatingAxis(Other, *this);
}

bool FInventoryItemProfile::IsValid() const
{
	if (Id.IsNone() || !::IsValid(Definition.Get()) || !Definition->IsValidDefinition() || ShapeParts.IsEmpty() || ShapeParts.Num() > 64) return false;
	FVector2D Min(TNumericLimits<double>::Max()), Max(-TNumericLimits<double>::Max());
	for (const FInventoryShapePart& Part : ShapeParts)
	{
		if (!Part.IsValid()) return false;
		for (FVector2D P : Part.Vertices)
		{
			Min.X = FMath::Min(Min.X, P.X); Min.Y = FMath::Min(Min.Y, P.Y);
			Max.X = FMath::Max(Max.X, P.X); Max.Y = FMath::Max(Max.Y, P.Y);
		}
	}
	return (Min + Max).Equals(FVector2D::ZeroVector, InventoryGeometry::Tolerance);
}

TArray<FInventoryShapePart> FInventoryItemProfile::GetTransformedParts(FVector2D Center, double AngleDegrees) const
{
	TArray<FInventoryShapePart> Parts = ShapeParts;
	const double Radians = FMath::DegreesToRadians(InventoryGeometry::NormalizeAngle(AngleDegrees));
	const double Cos = FMath::Cos(Radians), Sin = FMath::Sin(Radians);
	for (FInventoryShapePart& Part : Parts)
		for (FVector2D& P : Part.Vertices) P = Center + FVector2D(Cos * P.X - Sin * P.Y, Sin * P.X + Cos * P.Y);
	return Parts;
}

bool FInventoryItemProfile::Contains(FVector2D Point, FVector2D Center, double AngleDegrees) const
{
	for (const auto& Part : GetTransformedParts(Center, AngleDegrees)) if (Part.Contains(Point)) return true;
	return false;
}
