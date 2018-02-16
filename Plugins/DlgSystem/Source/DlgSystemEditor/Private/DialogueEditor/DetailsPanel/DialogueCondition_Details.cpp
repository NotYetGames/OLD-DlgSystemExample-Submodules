// Copyright 2017-2018 Csaba Molnar, Daniel Butum
#include "DialogueCondition_Details.h"

#include "IDetailPropertyRow.h"
#include "PropertyEditing.h"

#include "DlgNode.h"
#include "DialogueDetailsPanelUtils.h"
#include "DialogueEditor/Nodes/DialogueGraphNode.h"
#include "STextPropertyPickList.h"
#include "IPropertyUtilities.h"
#include "CustomRowHelpers/TextPropertyPickList_CustomRowHelper.h"

#define LOCTEXT_NAMESPACE "DialogueCondition_Details"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDialogueCondition_Details
void FDialogueCondition_Details::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;
	Dialogue = DetailsPanel::GetDialogueFromPropertyHandle(StructPropertyHandle.ToSharedRef());
	PropertyUtils = StructCustomizationUtils.GetPropertyUtilities();

	// Cache the Property Handle for some properties
	ParticipantNamePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgCondition, ParticipantName));
	ConditionTypePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgCondition, ConditionType));
	IntValuePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgCondition, IntValue));
	check(ParticipantNamePropertyHandle.IsValid());
	check(ConditionTypePropertyHandle.IsValid());
	check(IntValuePropertyHandle.IsValid());

	// Register handler propeties changes
	ConditionTypePropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &Self::OnConditionTypeChanged, true));

	const bool bShowOnlyInnerProperties = StructPropertyHandle->GetProperty()->HasMetaData(META_ShowOnlyInnerProperties);
	if (!bShowOnlyInnerProperties)
	{
		HeaderRow.NameContent()
			[
				StructPropertyHandle->CreatePropertyNameWidget()
			];
	}
}

void FDialogueCondition_Details::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Add common ConditionStrength, ConditionType
	StructBuilder.AddProperty(StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgCondition, Strength)).ToSharedRef());
	StructBuilder.AddProperty(ConditionTypePropertyHandle.ToSharedRef());

	// ParticipantName
	{
		FDetailWidgetRow* DetailWidgetRow = &StructBuilder.AddCustomRow(LOCTEXT("ParticipantNameSearchKey", "Participant Name"));

		ParticipantNamePropertyRow = MakeShareable(new FTextPropertyPickList_CustomRowHelper(DetailWidgetRow, ParticipantNamePropertyHandle));
		ParticipantNamePropertyRow->SetTextPropertyPickListWidget(
			SNew(STextPropertyPickList)
			.AvailableSuggestions(this, &Self::GetAllDialoguesParticipantNames)
			.OnTextCommitted(this, &Self::HandleTextCommitted)
			.HasContextCheckbox(true)
			.IsContextCheckBoxChecked(true)
			.CurrentContextAvailableSuggestions(this, &Self::GetCurrentDialogueParticipantNames)
		)
		->SetVisibility(CREATE_VISIBILITY_CALLBACK(&Self::GetParticipantNameVisibility))
		->Update();
	}

	// CallbackName (variable name)
	{
		const TSharedPtr<IPropertyHandle> CallbackNamePropertyHandle =
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgCondition, CallbackName));
		FDetailWidgetRow* DetailWidgetRow = &StructBuilder.AddCustomRow(LOCTEXT("CalllBackNameSearchKey", "Variable Name"));

		CallbackNamePropertyRow = MakeShareable(new FTextPropertyPickList_CustomRowHelper(DetailWidgetRow, CallbackNamePropertyHandle));
		CallbackNamePropertyRow->SetTextPropertyPickListWidget(
			SNew(STextPropertyPickList)
			.AvailableSuggestions(this, &Self::GetAllDialoguesCallbackNames)
			.OnTextCommitted(this, &Self::HandleTextCommitted)
			.HasContextCheckbox(true)
			.IsContextCheckBoxChecked(false)
			.CurrentContextAvailableSuggestions(this, &Self::GetCurrentDialogueCallbackNames)
		)
		->SetVisibility(CREATE_VISIBILITY_CALLBACK(&Self::GetCallbackNameVisibility))
		->Update();
	}

	// Operation
	{
		OperationPropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgCondition, Operation)).ToSharedRef());
		OperationPropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetOperationVisibility));
	}

	// IntValue
	{
		IntValuePropertyRow = &StructBuilder.AddProperty(IntValuePropertyHandle.ToSharedRef());
		IntValuePropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetIntValueVisibility));
	}

	// FloatValue
	{
		FloatValuePropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgCondition, FloatValue)).ToSharedRef());
		FloatValuePropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetFloatValueVisibility));
	}

	// NameValue
	{
		NameValuePropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgCondition, NameValue)).ToSharedRef());
		NameValuePropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetNameValueVisibility));
	}

	// bBoolValue
	{
		BoolValuePropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgCondition, bBoolValue)).ToSharedRef());
		BoolValuePropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetBoolValueVisibility));
	}

	// bLongTermMemory
	{
		LongTermMemoryPropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgCondition, bLongTermMemory)).ToSharedRef());
		LongTermMemoryPropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetLongTermMemoryVisibility));
	}

	// Cache the initial condition type
	OnConditionTypeChanged(false);
}

void FDialogueCondition_Details::OnConditionTypeChanged(bool bForceRefresh)
{
	// Update to the new type
	uint8 value;
	verify(ConditionTypePropertyHandle->GetValue(value) == FPropertyAccess::Success);
	ConditionType = static_cast<EDlgConditionType>(value);

	// Update the display names/tooltips
	FText CalllBackNameDisplayName = LOCTEXT("CalllBackNameDisplayName", "Variable Name");
	FText CalllBackNameToolTip = LOCTEXT("CalllBackNameToolTip", "The name of the checked variable");
	FText BoolValueDisplayName = LOCTEXT("BoolValueDisplayName", "Return Value");
	FText BoolValueToolTip = LOCTEXT("BoolValueToolTip", "SHOULD NOT BE VISIBLE");
	// TODO remove the "equal" operations for float values as they are imprecise
	FText FloatValueToolTip = LOCTEXT("FloatValueToolTip", "The float value the VariableName is checked against (depending on the operation).\n"
		"VariableName <Operation> FloatValue");
	FText IntValueDisplayName = LOCTEXT("IntValueDisplayName", "Int Value");
	FText IntValueToolTip = LOCTEXT("IntValueToolTip", "The int value the VariableName is checked against (depending on the operation).\n"
		"VariableName <Operation> IntValue");

	DetailsPanel::ResetNumericPropertyLimits(IntValuePropertyHandle);
	switch (ConditionType)
	{
	case EDlgConditionType::DlgConditionEventCall:
		CalllBackNameDisplayName = LOCTEXT("ConditionEvent_CallBackNameDisplayName", "Condition Name");
		CalllBackNameToolTip = LOCTEXT("ConditionEvent_CallBackNameToolTip", "Name parameter of the event call the participant gets");
		BoolValueToolTip = LOCTEXT("ConditionEvent_BoolValueToolTip", "Does the return result of the Event/Condition has this boolean value?");
		break;

	case EDlgConditionType::DlgConditionBoolCall:
		BoolValueToolTip = LOCTEXT("ConditionBool_BoolValueToolTip", "Does the VariableName equal this bool value?");
		break;

	case EDlgConditionType::DlgConditionNameCall:
		BoolValueToolTip = LOCTEXT("ConditionBool_BoolValueToolTip", "Should the Variable be checked if it equals this Name value?");
		BoolValueDisplayName = LOCTEXT("BoolValueDisplayName", "Succeed on Equal");
		break;

	case EDlgConditionType::DlgConditionFloatCall:
		break;
	case EDlgConditionType::DlgConditionIntCall:
		break;

	case EDlgConditionType::DlgConditionNodeVisited:
		DetailsPanel::SetNumericPropertyLimits<int32>(IntValuePropertyHandle, 0, Dialogue->GetNodes().Num() - 1);

		IntValueDisplayName = LOCTEXT("ConditionNodeVisited_IntValueDisplayName", "Node Index");
		IntValueToolTip = LOCTEXT("ConditionNodeVisited_IntValueToolTip", "Node index of the node we want to check the visited status");
		BoolValueDisplayName = LOCTEXT("ConditionNodeVisited_BoolValueDisplayName", "Is Node Visited?");
		BoolValueToolTip = LOCTEXT("ConditionNodeVisited_BoolValueToolTip", "Should the node be visited? True/False.");
		break;

	default:
		checkNoEntry();
	}

	CallbackNamePropertyRow->SetDisplayName(CalllBackNameDisplayName)
		->SetToolTip(CalllBackNameToolTip)
		->Update();

	BoolValuePropertyRow->DisplayName(BoolValueDisplayName);
	BoolValuePropertyRow->ToolTip(BoolValueToolTip);

	IntValuePropertyRow->DisplayName(IntValueDisplayName);
	IntValuePropertyRow->ToolTip(IntValueToolTip);
	FloatValuePropertyRow->ToolTip(FloatValueToolTip);

	// Refresh the view, without this some names/tooltips won't get refreshed
	if (bForceRefresh && PropertyUtils.IsValid())
	{
		PropertyUtils->ForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE