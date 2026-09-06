param(
    [Parameter(Mandatory=$true)][string]$SnapshotPath,
    [string]$EvidencePath = ""
)

$snapshot = Get-Content -Raw -LiteralPath $SnapshotPath | ConvertFrom-Json
$project = $snapshot.public_project
if ($null -eq $project) { throw "public_project is missing" }

$activeEscrow = 0
foreach ($order in @($project.orders)) {
    if ($order.status -eq "transporting") { $activeEscrow += [int]$order.escrow }
}
$result = [ordered]@{
    checked_at = (Get-Date).ToUniversalTime().ToString("o")
    backend = $snapshot.backend
    api_status = $snapshot.api_status
    model = $snapshot.model
    policy = $project.policy
    policy_scope = if ($project.policy -eq "local_king_fixed_income_tax_25") { "local policy" } else { "unknown policy" }
    project_status = $project.status
    project_id = $project.id
    site = $project.site
    king = $project.king
    completed = $project.completed
    part_count = @($project.parts).Count
    order_count = @($project.orders).Count
    active_escrow_snapshot = [int]$project.active_escrow
    active_escrow_recomputed = $activeEscrow
    escrow_conserved = ([int]$project.active_escrow -eq $activeEscrow)
    general_funds = [int]$snapshot.general_funds
    protected_funds = [int]$snapshot.protected_funds
    total_active_escrow = [int]$snapshot.total_active_escrow
    runtime_evidence_passed = ($project.status -eq "completed" -and [int]$project.completed -eq 15 -and @($project.parts).Count -eq 15 -and $activeEscrow -eq 0)
}
if ($EvidencePath) {
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $EvidencePath -Encoding utf8
}
$result | ConvertTo-Json -Depth 8
