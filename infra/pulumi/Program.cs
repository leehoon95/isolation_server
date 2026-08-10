using System.Collections.Generic;
using Pulumi;
using Pulumi.Aws.Ec2;
using Pulumi.Aws.Ec2.Inputs;
using Aws = Pulumi.Aws;

return await Deployment.RunAsync(() =>
{
    var config = new Config();

    // ---------------------------------------------------------------------
    // 설정값 (pulumi config set 으로 지정)
    //   pulumi config set instanceType t3.small
    //   pulumi config set sshPublicKey "ssh-ed25519 AAAA... your-key-comment"
    //   pulumi config set sshAllowedCidr "0.0.0.0/0"   # 나중에 러너 IP로 제한 권장
    // ---------------------------------------------------------------------
    var instanceType = config.Get("instanceType") ?? "t3.small";
    var sshPublicKey = config.RequireSecret("sshPublicKey"); // 공개키는 secret 취급 안 해도 되지만 실수 유출 방지 차원
    var sshAllowedCidr = config.Get("sshAllowedCidr") ?? "0.0.0.0/0";

    // ---------------------------------------------------------------------
    // 최신 Ubuntu 24.04 (Noble) AMI 조회 (Canonical 공식 계정 owner id)
    // ---------------------------------------------------------------------
    var ubuntuAmi = Aws.Ec2.GetAmi.Invoke(new Aws.Ec2.GetAmiInvokeArgs
    {
        MostRecent = true,
        Owners = new List<string> { "984445751003" }, // Canonical
        Filters = new List<Aws.Ec2.Inputs.GetAmiFilterInputArgs>
        {
            new()
            {
                Name = "name",
                Values = new List<string> { "ubuntu/images/hvm-ssd-gp3/ubuntu-noble-24.04-amd64-server-*" },
            },
            new()
            {
                Name = "virtualization-type",
                Values = new List<string> { "hvm" },
            },
            new()
            {
                Name = "root-device-type",
                Values = new List<string> { "ebs" },
            },
        },
    });

    // ---------------------------------------------------------------------
    // SSH 키페어 (개인키는 로컬에 보관, 여기서는 공개키만 등록)
    // ---------------------------------------------------------------------
    var keyPair = new KeyPair("ec2-keypair", new KeyPairArgs
    {
        PublicKey = sshPublicKey,
    });

    // ---------------------------------------------------------------------
    // 보안 그룹: SSH(22) 인바운드 + 전체 아웃바운드 허용
    // ---------------------------------------------------------------------
    var secGroup = new SecurityGroup("ec2-sg", new SecurityGroupArgs
    {
        Description = "Allow SSH inbound, all outbound",
        Ingress = new[]
        {
            new SecurityGroupIngressArgs
            {
                Protocol = "tcp",
                FromPort = 22,
                ToPort = 22,
                CidrBlocks = new[] { sshAllowedCidr },
                Description = "SSH",
            },
        },
        Egress = new[]
        {
            new SecurityGroupEgressArgs
            {
                Protocol = "-1",
                FromPort = 0,
                ToPort = 0,
                CidrBlocks = new[] { "0.0.0.0/0" },
            },
        },
        Tags = new Dictionary<string, string>
        {
            ["Name"] = "ec2-ssh-sg",
        },
    });

    // ---------------------------------------------------------------------
    // UserData: 부팅 시 Docker 설치 (Ubuntu 공식 apt 저장소 사용)
    // ---------------------------------------------------------------------
    const string userData = @"#!/bin/bash
        mkdir created_$(date '+%Y-%m-%d_%H%M%S')";

    // set -eux

    // apt-get update -y
    // apt-get install -y ca-certificates curl gnupg

    // install -m 0755 -d /etc/apt/keyrings
    // curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
    // chmod a+r /etc/apt/keyrings/docker.asc

    // echo \
    //   ""deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
    //   $(. /etc/os-release && echo ""$VERSION_CODENAME"") stable"" | \
    //   tee /etc/apt/sources.list.d/docker.list > /dev/null

    // apt-get update -y
    // apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

    // usermod -aG docker ubuntu

    // systemctl enable docker
    // systemctl start docker

    // ---------------------------------------------------------------------
    // EC2 인스턴스
    // ---------------------------------------------------------------------
    var instance = new Instance("ubuntu-docker-instance", new InstanceArgs
    {
        InstanceType = instanceType,
        Ami = ubuntuAmi.Apply(a => a.Id),
        KeyName = keyPair.KeyName,
        VpcSecurityGroupIds = new[] { secGroup.Id },
        UserData = userData,
        AssociatePublicIpAddress = true,
        RootBlockDevice = new InstanceRootBlockDeviceArgs
        {
            VolumeSize = 20,
            VolumeType = "gp3",
        },
        Tags = new Dictionary<string, string>
        {
            ["wakeup"] = "weekday",
        },
    });

    return new Dictionary<string, object?>
    {
        ["instanceId"] = instance.Id,
        ["publicIp"] = instance.PublicIp,
        ["publicDns"] = instance.PublicDns,
        ["sshCommand"] = Output.Format($"ssh -i <private-key-path> ubuntu@{instance.PublicIp}"),
    };
});