// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Units/VeyraUnit.h"

namespace VeyraUnits
{
TOptional<EVeyraUnitKind> KindOf(const UObject* Object)
{
	const IVeyraUnit* Unit = Cast<IVeyraUnit>(Object);
	return Unit ? TOptional<EVeyraUnitKind>(Unit->GetVeyraUnitKind()) : TOptional<EVeyraUnitKind>();
}

bool IsVanguard(const UObject* Object)
{
	const TOptional<EVeyraUnitKind> Kind = KindOf(Object);
	return Kind.IsSet() && Kind.GetValue() == EVeyraUnitKind::Vanguard;
}

bool IsStructure(const UObject* Object)
{
	const TOptional<EVeyraUnitKind> Kind = KindOf(Object);
	return Kind.IsSet() && Kind.GetValue() == EVeyraUnitKind::Structure;
}

bool IsWard(const UObject* Object)
{
	const TOptional<EVeyraUnitKind> Kind = KindOf(Object);
	return Kind.IsSet() && Kind.GetValue() == EVeyraUnitKind::Ward;
}

bool IsMarker(const UObject* Object)
{
	const TOptional<EVeyraUnitKind> Kind = KindOf(Object);
	return Kind.IsSet() && Kind.GetValue() == EVeyraUnitKind::Marker;
}

bool IsNeutral(const UObject* Object)
{
	const TOptional<EVeyraUnitKind> Kind = KindOf(Object);
	return Kind.IsSet() && (Kind.GetValue() == EVeyraUnitKind::Wildlife || Kind.GetValue() == EVeyraUnitKind::Objective);
}
}
